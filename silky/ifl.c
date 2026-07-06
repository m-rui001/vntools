#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "lzss.h"
#include "readint.h"
#include "writeint.h"

#define PROG "ifl"

struct ifl_entry
{
	unsigned long offset;
	unsigned long size;
	char filename[17];
};

static int unpack_entry(FILE *ifl, const struct ifl_entry *entry)
{
	char *ext;
	FILE *out;
	unsigned long size = entry->size;

	fseek(ifl, entry->offset, SEEK_SET);

	ext = strrchr(entry->filename, '.');
	if(ext && strcmp(ext, ".grd") == 0)
	{
		char out_name[17];
		unsigned char header[12];
		unsigned char *compressed, *raw;
		unsigned long compressed_size, raw_size;

		strcpy(out_name, entry->filename);
		ext = strrchr(out_name, '.');
		memcpy(ext, ".bmp", 5);

		if(fread(header, 1, 12, ifl) != 12)
		{
			fprintf(stderr, PROG ": failed to read header for %s\n", entry->filename);
			return -1;
		}
		raw_size = read_uint32_le(&header[4]);
		compressed_size = size - 12;

		compressed = malloc(compressed_size);
		raw = malloc(raw_size);
		if(!compressed || !raw)
		{
			fprintf(stderr, PROG ": out of memory for %s\n", entry->filename);
			free(compressed);
			free(raw);
			return -1;
		}

		if(fread(compressed, 1, compressed_size, ifl) != compressed_size)
		{
			fprintf(stderr, PROG ": read error for %s\n", entry->filename);
			free(compressed);
			free(raw);
			return -1;
		}

		if(lzss_decompress(raw, raw_size, compressed, compressed_size) != 0)
		{
			fprintf(stderr, PROG ": failed to decompress %s\n", entry->filename);
			free(compressed);
			free(raw);
			return -1;
		}
		free(compressed);

		fprintf(stdout, "decompressing and extracting %s -> %s\n", entry->filename, out_name);

		out = fopen(out_name, "wb");
		if(!out)
		{
			fprintf(stderr, PROG ": failed to create %s\n", out_name);
			free(raw);
			return -1;
		}
		fwrite(raw, 1, raw_size, out);
		free(raw);
		fclose(out);
	}
	else
	{
		unsigned char *raw = malloc(size);

		if(!raw)
		{
			fprintf(stderr, PROG ": out of memory for %s\n", entry->filename);
			return -1;
		}
		if(fread(raw, 1, size, ifl) != size)
		{
			fprintf(stderr, PROG ": read error for %s\n", entry->filename);
			free(raw);
			return -1;
		}

		fprintf(stdout, "extracting %s\n", entry->filename);

		out = fopen(entry->filename, "wb");
		if(!out)
		{
			fprintf(stderr, PROG ": failed to create %s\n", entry->filename);
			free(raw);
			return -1;
		}
		fwrite(raw, 1, size, out);
		free(raw);
		fclose(out);
	}

	return 0;
}

int unpack_ifl(const char *in_path)
{
	int i;
	unsigned int count;
	FILE *ifl, *lst;
	unsigned char header_buf[12];
	static const unsigned char ifl_magic[4] = {0x49, 0x46, 0x4c, 0x53};
	struct ifl_entry *entries;
	int had_error = 0;

	ifl = fopen(in_path, "rb");
	if(!ifl)
	{
		fprintf(stderr, PROG ": cannot open %s\n", in_path);
		return 1;
	}

	if(fread(header_buf, 1, 12, ifl) != 12 || memcmp(header_buf, ifl_magic, 4) != 0)
	{
		fprintf(stderr, PROG ": %s is not an IFL archive\n", in_path);
		fclose(ifl);
		return 1;
	}

	count = read_uint32_le(&header_buf[8]);

	entries = calloc(count, sizeof(struct ifl_entry));
	if(!entries)
	{
		fprintf(stderr, PROG ": out of memory\n");
		fclose(ifl);
		return 1;
	}

	lst = fopen("list.txt", "wb");
	if(!lst)
	{
		fprintf(stderr, PROG ": cannot create list.txt\n");
		free(entries);
		fclose(ifl);
		return 1;
	}

	for(i = 0; i < (int)count; i++)
	{
		unsigned char buf[24];

		if(fread(buf, 1, 24, ifl) != 24)
		{
			fprintf(stderr, PROG ": truncated entry table in %s\n", in_path);
			fclose(lst);
			free(entries);
			fclose(ifl);
			return 1;
		}
		memcpy(entries[i].filename, buf, 16);
		entries[i].offset = read_uint32_le(&buf[16]);
		entries[i].size = read_uint32_le(&buf[20]);

		fprintf(lst, "%s\n", entries[i].filename);
	}

	fclose(lst);
	fclose(ifl);

	#pragma omp parallel for
	for(i = 0; i < (int)count; i++)
	{
		FILE *ifl_thread = fopen(in_path, "rb");

		if(!ifl_thread)
		{
			fprintf(stderr, PROG ": cannot reopen %s\n", in_path);
			had_error = 1;
			continue;
		}
		if(unpack_entry(ifl_thread, &entries[i]) != 0)
			had_error = 1;
		fclose(ifl_thread);
	}

	free(entries);
	return had_error;
}

int pack_ifl(const char *out_path, const char *list_path)
{
	char *p;
	char filename[18];
	long ifl_pos, entry_size;
	unsigned int count, data_offset;
	static const unsigned char magic[4] = {0x49, 0x46, 0x4c, 0x53};
	FILE *ifl, *entry, *lst;

	lst = fopen(list_path, "rb");
	if(!lst)
	{
		fprintf(stderr, PROG ": cannot open %s\n", list_path);
		return 1;
	}

	ifl = fopen(out_path, "wb");
	if(!ifl)
	{
		fprintf(stderr, PROG ": cannot create %s\n", out_path);
		fclose(lst);
		return 1;
	}

	fwrite(magic, 1, 4, ifl);

	count = 0;
	while(fgets(filename, sizeof(filename), lst))
		count++;
	rewind(lst);

	data_offset = (count * 24) + 12;
	fwrite_uint32_le(data_offset, ifl);
	fwrite_uint32_le(count, ifl);

	while(1)
	{
		memset(filename, 0, sizeof(filename));
		if(!fgets(filename, sizeof(filename), lst))
			break;

		if((p = strrchr(filename, '\r')))
			*p = '\0';
		if((p = strrchr(filename, '\n')))
			*p = '\0';

		entry = fopen(filename, "rb");
		if(!entry)
		{
			fprintf(stderr, PROG ": %s doesn't exist\n", filename);
			fclose(ifl);
			fclose(lst);
			return 1;
		}

		fseek(entry, 0, SEEK_END);
		entry_size = ftell(entry);
		rewind(entry);

		fwrite(filename, 1, 16, ifl);
		fwrite_uint32_le(data_offset, ifl);

		ifl_pos = ftell(ifl);

		fprintf(stdout, "writing %s to archive\n", filename);

		{
			char copybuf[8192];
			size_t n;

			fseek(ifl, data_offset, SEEK_SET);
			while((n = fread(copybuf, 1, sizeof(copybuf), entry)) > 0)
				fwrite(copybuf, 1, n, ifl);
		}

		fseek(ifl, ifl_pos, SEEK_SET);
		data_offset += (unsigned int)entry_size;
		fwrite_uint32_le((uint32_t)entry_size, ifl);
		fclose(entry);
	}

	fclose(lst);
	fclose(ifl);
	return 0;
}

void print_usage(const char *progname)
{
	fprintf(stderr, "IFL tool - pack and unpack Silky IFL archives\n\n");
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  Unpack: %s [-u|--unpack] <archive.ifl>\n", progname);
	fprintf(stderr, "  Pack:   %s [-p|--pack] <archive.ifl> <list.txt>\n\n", progname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -u, --unpack    Extract archive entries and write list.txt\n");
	fprintf(stderr, "  -p, --pack      Pack files named in list.txt into archive\n");
	fprintf(stderr, "  -h, --help      Show this help message\n");
}

int main(int argc, char *argv[])
{
	if(argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
	{
		print_usage(argv[0]);
		return 0;
	}

	if(argc == 3 && (strcmp(argv[1], "-u") == 0 || strcmp(argv[1], "--unpack") == 0))
		return unpack_ifl(argv[2]);

	if(argc == 4 && (strcmp(argv[1], "-p") == 0 || strcmp(argv[1], "--pack") == 0))
		return pack_ifl(argv[2], argv[3]);

	print_usage(argv[0]);
	return 1;
}

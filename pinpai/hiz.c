#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "bmp_read.h"
#include "lzss.h"
#include "readint.h"
#include "writeint.h"

#define PROG "hiz"
#define BMP_HDR_SIZE 66u /* 14 file header + 40 BITMAPINFOHEADER + 12 BI_BITFIELDS masks */

int unpack_hiz(const char *in_path, const char *out_path);
int pack_hiz(const char *in_path, const char *out_path);
void print_usage(const char *progname);

static int write_bmp(FILE *f, const unsigned char *raw565, uint32_t w, uint32_t h)
{
	unsigned row_bytes = w * 2;
	unsigned pad = (4 - (row_bytes % 4)) % 4;
	unsigned stride = row_bytes + pad;
	uint32_t pixel_data_size = stride * h;
	uint32_t file_size = BMP_HDR_SIZE + pixel_data_size;
	uint32_t y;
	unsigned char hdr[BMP_HDR_SIZE];

	memset(hdr, 0, sizeof(hdr));
	hdr[0] = 'B';
	hdr[1] = 'M';
	hdr[2] = (unsigned char)(file_size);
	hdr[3] = (unsigned char)(file_size >> 8);
	hdr[4] = (unsigned char)(file_size >> 16);
	hdr[5] = (unsigned char)(file_size >> 24);
	hdr[10] = (unsigned char)(BMP_HDR_SIZE);
	hdr[14] = 40;
	hdr[18] = (unsigned char)(w);
	hdr[19] = (unsigned char)(w >> 8);
	hdr[20] = (unsigned char)(w >> 16);
	hdr[21] = (unsigned char)(w >> 24);
	hdr[22] = (unsigned char)(h);
	hdr[23] = (unsigned char)(h >> 8);
	hdr[24] = (unsigned char)(h >> 16);
	hdr[25] = (unsigned char)(h >> 24);
	hdr[26] = 1;
	hdr[28] = 16;
	hdr[30] = 3; /* biCompression = BI_BITFIELDS */
	hdr[34] = (unsigned char)(pixel_data_size);
	hdr[35] = (unsigned char)(pixel_data_size >> 8);
	hdr[36] = (unsigned char)(pixel_data_size >> 16);
	hdr[37] = (unsigned char)(pixel_data_size >> 24);
	/* R/G/B bitmasks for RGB565, appended after the BITMAPINFOHEADER */
	hdr[54] = 0x00; hdr[55] = 0xF8; hdr[56] = 0x00; hdr[57] = 0x00;
	hdr[58] = 0xE0; hdr[59] = 0x07; hdr[60] = 0x00; hdr[61] = 0x00;
	hdr[62] = 0x1F; hdr[63] = 0x00; hdr[64] = 0x00; hdr[65] = 0x00;

	if(fwrite(hdr, 1, BMP_HDR_SIZE, f) != BMP_HDR_SIZE)
		return -1;

	for(y = 0; y < h; y++)
	{
		uint32_t vis_row = h - 1u - y;
		const unsigned char *row = raw565 + (size_t)vis_row * w * 2;

		if(fwrite(row, 1, row_bytes, f) != row_bytes)
			return -1;
		if(pad && fwrite("\0\0\0", 1, pad, f) != pad)
			return -1;
	}

	return 0;
}

int unpack_hiz(const char *in_path, const char *out_path)
{
	FILE *in;
	unsigned char head[12];
	long file_sz;
	size_t payload;
	unsigned char *file_buf = NULL;
	unsigned char *raw = NULL;
	uint32_t w, h;
	size_t expected;

	in = fopen(in_path, "rb");
	if(!in)
	{
		fprintf(stderr, PROG ": cannot open %s\n", in_path);
		return 1;
	}

	if(fseek(in, 0, SEEK_END) != 0 || (file_sz = ftell(in)) < 0)
	{
		fprintf(stderr, PROG ": cannot size %s\n", in_path);
		fclose(in);
		return 1;
	}
	if(file_sz < 12)
	{
		fprintf(stderr, PROG ": %s too small\n", in_path);
		fclose(in);
		return 1;
	}
	rewind(in);
	if(fread(head, 1, 12, in) != 12)
	{
		fprintf(stderr, PROG ": read error %s\n", in_path);
		fclose(in);
		return 1;
	}

	w = read_uint32_le(&head[0]);
	h = read_uint32_le(&head[4]);

	if(w == 0 || h == 0)
	{
		fprintf(stderr, PROG ": invalid dimensions in %s\n", in_path);
		fclose(in);
		return 1;
	}

	payload = (size_t)file_sz - 12u;
	if(payload == 0)
	{
		fprintf(stderr, PROG ": no compressed payload in %s\n", in_path);
		fclose(in);
		return 1;
	}

	file_buf = malloc(payload);
	if(file_buf == NULL)
	{
		fprintf(stderr, PROG ": out of memory\n");
		fclose(in);
		return 1;
	}
	if(fread(file_buf, 1, payload, in) != payload)
	{
		fprintf(stderr, PROG ": read error %s\n", in_path);
		free(file_buf);
		fclose(in);
		return 1;
	}
	fclose(in);

	if((size_t)w > SIZE_MAX / 2u || (size_t)h > (SIZE_MAX / 2u) / (size_t)w)
	{
		fprintf(stderr, PROG ": dimension overflow for %s\n", in_path);
		free(file_buf);
		return 1;
	}
	expected = (size_t)w * (size_t)h * 2u;

	raw = malloc(expected);
	if(raw == NULL)
	{
		fprintf(stderr, PROG ": out of memory\n");
		free(file_buf);
		return 1;
	}

	if(lzss_decompress(raw, (unsigned long)expected, file_buf, (unsigned long)payload) != 0)
	{
		fprintf(stderr, PROG ": LZSS decompress failed for %s\n", in_path);
		free(file_buf);
		free(raw);
		return 1;
	}
	free(file_buf);

	{
		FILE *out = fopen(out_path, "wb");

		if(out == NULL)
		{
			fprintf(stderr, PROG ": cannot create %s\n", out_path);
			free(raw);
			return 1;
		}
		if(write_bmp(out, raw, w, h) != 0)
		{
			fprintf(stderr, PROG ": write error %s\n", out_path);
			fclose(out);
			free(raw);
			return 1;
		}
		fclose(out);
	}

	free(raw);
	fprintf(stderr, PROG ": %s -> %s\n", in_path, out_path);
	return 0;
}

static int bmp_to_raw565(const unsigned char *buf, size_t sz, unsigned char **raw_out, uint32_t *w_out, uint32_t *h_out)
{
	struct bmp_image bmp;
	unsigned char *raw;

	if(bmp_read_image(buf, sz, &bmp) != 0)
		return -1;

	if(bmp.bpp != 16u)
	{
		fprintf(stderr, PROG ": input BMP must be 16-bit RGB565\n");
		return -1;
	}

	raw = malloc((size_t)bmp.width * (size_t)bmp.height * 2u);
	if(raw == NULL)
	{
		fprintf(stderr, PROG ": out of memory\n");
		return -1;
	}

	bmp_decode_to_rgb565_le(&bmp, raw);

	*w_out = bmp.width;
	*h_out = bmp.height;
	*raw_out = raw;
	return 0;
}

int pack_hiz(const char *in_path, const char *out_path)
{
	FILE *in;
	unsigned char *file_buf = NULL;
	unsigned char *raw = NULL;
	unsigned char *compressed = NULL;
	long file_sz;
	uint32_t w, h;
	size_t raw_len;
	unsigned long bound;
	int clen;

	in = fopen(in_path, "rb");
	if(in == NULL)
	{
		fprintf(stderr, PROG ": cannot open %s\n", in_path);
		return 1;
	}

	if(fseek(in, 0, SEEK_END) != 0 || (file_sz = ftell(in)) < 0)
	{
		fprintf(stderr, PROG ": cannot read %s\n", in_path);
		fclose(in);
		return 1;
	}
	rewind(in);

	if(file_sz <= 0 || (size_t)file_sz > SIZE_MAX / 2u)
	{
		fprintf(stderr, PROG ": unreasonable file size %s\n", in_path);
		fclose(in);
		return 1;
	}

	file_buf = malloc(file_sz);
	if(file_buf == NULL)
	{
		fprintf(stderr, PROG ": out of memory\n");
		fclose(in);
		return 1;
	}

	if(fread(file_buf, 1, (size_t)file_sz, in) != (size_t)file_sz)
	{
		fprintf(stderr, PROG ": read error %s\n", in_path);
		free(file_buf);
		fclose(in);
		return 1;
	}
	fclose(in);

	if(bmp_to_raw565(file_buf, (size_t)file_sz, &raw, &w, &h) != 0)
	{
		free(file_buf);
		return 1;
	}
	free(file_buf);

	raw_len = (size_t)w * (size_t)h * 2u;
	bound = lzss_compress_bound((unsigned long)raw_len);
	if(bound < raw_len)
		bound = raw_len;

	compressed = malloc(bound);
	if(compressed == NULL)
	{
		fprintf(stderr, PROG ": out of memory\n");
		free(raw);
		return 1;
	}

	clen = lzss_compress(compressed, bound, raw, (unsigned long)raw_len);
	free(raw);

	if(clen < 0)
	{
		fprintf(stderr, PROG ": LZSS compress failed for %s\n", in_path);
		free(compressed);
		return 1;
	}

	{
		FILE *out = fopen(out_path, "wb");

		if(out == NULL)
		{
			fprintf(stderr, PROG ": cannot create %s\n", out_path);
			free(compressed);
			return 1;
		}

		if(fwrite_uint32_le(w, out) != 4 || fwrite_uint32_le(h, out) != 4 ||
		   fwrite_uint32_le((uint32_t)clen, out) != 4 ||
		   fwrite(compressed, 1, (size_t)clen, out) != (size_t)clen)
		{
			fprintf(stderr, PROG ": write error %s\n", out_path);
			fclose(out);
			free(compressed);
			return 1;
		}
		fclose(out);
	}

	free(compressed);
	fprintf(stderr, PROG ": %s -> %s\n", in_path, out_path);
	return 0;
}

void print_usage(const char *progname)
{
	fprintf(stderr, "HIZ tool - pack and unpack Pinpai HIZ images\n\n");
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  Unpack: %s [-u|--unpack] <file.hiz>\n", progname);
	fprintf(stderr, "  Pack:   %s [-p|--pack] <file.bmp>\n\n", progname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -u, --unpack    Decompress HIZ to 16-bit RGB565 BMP\n");
	fprintf(stderr, "  -p, --pack      Compress a 16-bit RGB565 BMP to HIZ\n");
	fprintf(stderr, "  -h, --help      Show this help message\n");
}

int main(int argc, char *argv[])
{
	char out_path[PATH_MAX];
	char *ext;

	if(argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
	{
		print_usage(argv[0]);
		return 0;
	}

	if(argc != 3)
	{
		print_usage(argv[0]);
		return 1;
	}

	if(strlen(argv[2]) < 3 || strlen(argv[2]) >= sizeof(out_path))
	{
		fprintf(stderr, PROG ": bad path: %s\n", argv[2]);
		return 1;
	}
	strcpy(out_path, argv[2]);

	ext = out_path + strlen(out_path) - 3;

	if(strcmp(argv[1], "-u") == 0 || strcmp(argv[1], "--unpack") == 0)
	{
		memcpy(ext, "bmp", 3);
		return unpack_hiz(argv[2], out_path);
	}

	if(strcmp(argv[1], "-p") == 0 || strcmp(argv[1], "--pack") == 0)
	{
		memcpy(ext, "hiz", 3);
		return pack_hiz(argv[2], out_path);
	}

	print_usage(argv[0]);
	return 1;
}

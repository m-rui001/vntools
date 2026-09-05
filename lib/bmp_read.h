#ifndef BMP_READ_H
#define BMP_READ_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct bmp_image
{
	const unsigned char *pixels;
	uint32_t width;
	uint32_t height;
	uint16_t bpp;
	uint32_t stride;
	int top_down;
};

static uint16_t bmp_read_u16_le(const unsigned char *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t bmp_read_u32_le(const unsigned char *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int bmp_read_image(const unsigned char *buf, size_t sz, struct bmp_image *bmp)
{
	uint32_t pixel_offset, dib_size, width, height_u, compression, stride, planes, bpp;
	int32_t height;
	size_t pixel_bytes;

	if(buf == NULL || bmp == NULL || sz < 54)
		return -1;

	if(buf[0] != 'B' || buf[1] != 'M')
		return -1;

	pixel_offset = bmp_read_u32_le(&buf[10]);
	dib_size = bmp_read_u32_le(&buf[14]);
	width = bmp_read_u32_le(&buf[18]);
	height = (int32_t)bmp_read_u32_le(&buf[22]);
	planes = bmp_read_u16_le(&buf[26]);
	bpp = bmp_read_u16_le(&buf[28]);
	compression = bmp_read_u32_le(&buf[30]);

	if(dib_size < 40 || planes != 1 || bpp != 16 || width == 0 || height == 0)
		return -1;

	if(compression == 3)
	{
		size_t mask_offset = 14u + (size_t)dib_size;
		if(mask_offset + 12u > sz)
			return -1;
		if(bmp_read_u32_le(&buf[mask_offset]) != 0x0000f800u ||
		   bmp_read_u32_le(&buf[mask_offset + 4u]) != 0x000007e0u ||
		   bmp_read_u32_le(&buf[mask_offset + 8u]) != 0x0000001fu)
			return -1;
	}
	else if(compression != 0)
	{
		return -1;
	}

	height_u = (height < 0) ? (uint32_t)(-height) : (uint32_t)height;
	if(width > (UINT32_MAX - 3u) / 2u)
		return -1;
	stride = ((width * 2u) + 3u) & ~3u;

	pixel_bytes = (size_t)stride * (size_t)height_u;
	if(pixel_offset > sz || pixel_bytes > (sz - pixel_offset))
		return -1;

	bmp->pixels = buf + pixel_offset;
	bmp->width = width;
	bmp->height = height_u;
	bmp->bpp = (uint16_t)bpp;
	bmp->stride = stride;
	bmp->top_down = (height < 0);

	return 0;
}

static void bmp_decode_to_rgb565_le(const struct bmp_image *bmp, unsigned char *raw)
{
	uint32_t y;

	for(y = 0; y < bmp->height; y++)
	{
		uint32_t src_y = bmp->top_down ? y : (bmp->height - 1u - y);
		const unsigned char *src = bmp->pixels + (size_t)src_y * bmp->stride;
		unsigned char *dst = raw + ((size_t)y * (size_t)bmp->width * 2u);

		memcpy(dst, src, (size_t)bmp->width * 2u);
	}
}

#endif

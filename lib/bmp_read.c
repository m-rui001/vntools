#include <string.h>

#include "bmp_read.h"
#include "readint.h"

int bmp_read_image(const unsigned char *buf, size_t sz, struct bmp_image *bmp)
{
	uint32_t dib_size, offset, width, height_abs, row_bits, stride;
	int32_t height;
	uint16_t planes, bpp;
	uint32_t compression;

	if(sz < 54 || buf[0] != 'B' || buf[1] != 'M')
		return -1;

	offset = read_uint32_le(&buf[10]);
	dib_size = read_uint32_le(&buf[14]);
	if(dib_size < 40 || (size_t)14u + dib_size > sz)
		return -1;

	width = read_uint32_le(&buf[18]);
	height = (int32_t)read_uint32_le(&buf[22]);
	planes = read_uint16_le(&buf[26]);
	bpp = read_uint16_le(&buf[28]);
	compression = read_uint32_le(&buf[30]);

	if(width == 0 || height == 0 || planes != 1 || bpp != 16)
		return -1;

	if(height < 0)
		height_abs = (uint32_t)(-height);
	else
		height_abs = (uint32_t)height;

	row_bits = width * (uint32_t)bpp;
	stride = ((row_bits + 31u) / 32u) * 4u;
	if(stride < width * 2u)
		return -1;
	if((size_t)stride * height_abs > sz || offset > sz || (size_t)offset + (size_t)stride * height_abs > sz)
		return -1;

	if(compression == 3)
	{
		const unsigned char *masks = &buf[14u + dib_size];
		if((size_t)(14u + dib_size + 12u) > sz)
			return -1;
		bmp->r_mask = read_uint32_le(&masks[0]);
		bmp->g_mask = read_uint32_le(&masks[4]);
		bmp->b_mask = read_uint32_le(&masks[8]);
	}
	else if(compression == 0)
	{
		/* default BI_RGB 16-bit mask is generally RGB555 */
		bmp->r_mask = 0x00007c00u;
		bmp->g_mask = 0x000003e0u;
		bmp->b_mask = 0x0000001fu;
	}
	else
		return -1;

	if(bmp->r_mask != 0x0000f800u || bmp->g_mask != 0x000007e0u || bmp->b_mask != 0x0000001fu)
		return -1;

	bmp->pixels = &buf[offset];
	bmp->width = width;
	bmp->height = height_abs;
	bmp->stride = stride;
	bmp->bpp = bpp;
	bmp->top_down = (height < 0) ? 1 : 0;

	return 0;
}

void bmp_decode_to_rgb565_le(const struct bmp_image *bmp, unsigned char *raw565_le)
{
	uint32_t y;

	for(y = 0; y < bmp->height; y++)
	{
		uint32_t src_y = bmp->top_down ? y : (bmp->height - 1u - y);
		const unsigned char *src = bmp->pixels + (size_t)src_y * bmp->stride;
		unsigned char *dst = raw565_le + ((size_t)y * bmp->width * 2u);

		memcpy(dst, src, (size_t)bmp->width * 2u);
	}
}

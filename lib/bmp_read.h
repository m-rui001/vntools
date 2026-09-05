#ifndef BMP_READ_H
#define BMP_READ_H

#include <stddef.h>
#include <stdint.h>

struct bmp_image
{
	const unsigned char *pixels;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t r_mask;
	uint32_t g_mask;
	uint32_t b_mask;
	unsigned int bpp;
	int top_down;
};

int bmp_read_image(const unsigned char *buf, size_t sz, struct bmp_image *bmp);
void bmp_decode_to_rgb565_le(const struct bmp_image *bmp, unsigned char *raw565_le);

#endif /* !BMP_READ_H */

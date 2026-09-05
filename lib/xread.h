#ifndef XREAD_H
#define XREAD_H

#include <stdio.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 8192
#endif

static size_t xread(unsigned char *buffer, size_t remaining, FILE *input)
{
	size_t chunk = remaining;

	if(chunk > BUFFER_SIZE)
		chunk = BUFFER_SIZE;

	return fread(buffer, 1, chunk, input);
}

#endif

#ifndef XREAD_H
#define XREAD_H

#include <stdio.h>
#include <stdlib.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 8192
#endif

static inline size_t xread(void *buf, size_t remaining, FILE *stream)
{
	size_t chunk = remaining > BUFFER_SIZE ? BUFFER_SIZE : remaining;
	size_t bytes_read = fread(buf, 1, chunk, stream);

	if(bytes_read == 0)
	{
		fprintf(stderr, "unexpected read issue\n");
		exit(EXIT_FAILURE);
	}

	return bytes_read;
}

#endif /* !XREAD_H */

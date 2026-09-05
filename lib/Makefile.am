noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = xmkdir.c xprintf.c cp932.c cp932table.c win32compat.c normalize.c swap.c lzss.c rgb565.c bmp_read.c

if NEED_STRCASECMP
libcommon_a_SOURCES += strcasecmp.c
endif

include_HEADERS = xmkdir.h xprintf.h cp932.h win32compat.h readint.h writeint.h normalize.h swap.h lzss.h strcasecmp.h xread.h rgb565.h bmp_read.h

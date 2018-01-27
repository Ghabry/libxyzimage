EXTRA_DIST = AUTHORS.md README.md TODO
pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = extra/libxyzimage.pc

lib_LTLIBRARIES = libxyzimage.la
libxyzimage_la_CFLAGS = \
	$(ZLIB_CFLAGS) \
	-I$(srcdir)/include
libxyzimage_la_LIBADD = \
	$(ZLIB_LIBS)
libxyzimage_la_SOURCES = \
	src/xyzimage.c
pkginclude_HEADERS = \
	include/xyzimage.h

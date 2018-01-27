AC_INIT([libxyzimage],[1.0],[https://github.com/EasyRPG/libxyzimage/issues],[libxyzimage],[https://easyrpg.org/])

AC_CONFIG_AUX_DIR([autoconf])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/xyzimage.c])
AC_CONFIG_HEADERS([config.h])
AC_LANG([C])

AM_PROG_AR
LT_INIT([win32-dll])

PKG_CHECK_MODULES([ZLIB],[zlib])

AC_CONFIG_FILES([Makefile
	extra/libxyzimage.pc])

AC_OUTPUT

AC_PREREQ(2.60)
AC_INIT([libsockmux-glib],
	[1],
	[sockmux@zonque.org],
	[libsockmux-glib],
	[http://github.com/...])
AC_CONFIG_SRCDIR([src/sender.c])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([check-news foreign 1.11 -Wall -Wno-portability silent-rules tar-pax dist-bzip2 subdir-objects])
AC_PROG_CC_STDC
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])
LT_INIT([disable-static pic-only])
AC_PREFIX_DEFAULT([/usr/local])

# Checks for libraries.
PKG_CHECK_MODULES(GLIB,		[ glib-2.0 >= 2.28.0,
				  gio-2.0 >= 2.28.0,
				  gio-unix-2.0 >= 2.28.0,
				  gobject-2.0 >= 2.28.0 ])
CFLAGS="$CFLAGS $GLIB_CFLAGS"
LDFLAGS="$LDFLAGS $GLIB_LIBS"

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([
	Makefile
	src/libsockmux-glib.pc
])

AC_OUTPUT
AC_MSG_RESULT([
	$PACKAGE $VERSION
	===================

	prefix:			${prefix}
	sysconfdir:		${sysconfdir}
	libdir:			${libdir}
	includedir:		${includedir}

	compiler:		${CC}
	cflags:			${CFLAGS}
	ldflags:		${LDFLAGS}

	debug:			${enable_debug}
])

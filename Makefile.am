EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-I$(top_srcdir)/src

AM_CFLAGS = \
	-ffunction-sections \
	-fdata-sections

AM_LDFLAGS = \
	-Wl,--gc-sections \
	-Wl,--as-needed

LIB_CURRENT=1
LIB_REVISION=0
LIB_AGE=1

include_HEADERS = src/libsockmux-glib.h
lib_LTLIBRARIES = src/libsockmux-glib.la

src_libsockmux_glib_la_SOURCES =\
	src/sender.h src/sender.c \
	src/receiver.h src/receiver.c \
	src/message.h

src_libsockmux_glib_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info $(LIB_CURRENT):$(LIB_REVISION):$(LIB_AGE)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = src/libsockmux-glib.pc

TESTS = test-libsockmux-glib
check_PROGRAMS = test-libsockmux-glib
test_libsockmux_glib_SOURCES = test-libsockmux-glib.c
test_libsockmux_glib_LDADD = src/libsockmux-glib.la

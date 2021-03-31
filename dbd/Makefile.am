
ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = dbd
dbd_SOURCES = dbd.c

dist_man_MANS = dbd.1

dbd.1: dbd.c $(top_srcdir)/configure.ac
	which txt2man && ./dbd --help | txt2man -d 1 -t "dbd" -r "${PACKAGE_NAME}-${PACKAGE_VERSION}" > dbd.1 || true



ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = nmbe
nmbe_SOURCES = nmbe.c

dist_man_MANS = nmbe.1

nmbe.1: nmbe.c $(top_srcdir)/configure.ac
	which txt2man && ./nmbe --help | txt2man -d 1 -t "nmbe" -r "${PACKAGE_NAME}-${PACKAGE_VERSION}" > nmbe.1 || true


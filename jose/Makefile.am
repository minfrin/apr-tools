
ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = jose
jose_SOURCES = jose.c

dist_man_MANS = jose.1

jose.1: jose.c $(top_srcdir)/configure.ac
	which txt2man && ./jose --help | txt2man -d 1 -t "jose" -r "${PACKAGE_NAME}-${PACKAGE_VERSION}" > jose.1 || true


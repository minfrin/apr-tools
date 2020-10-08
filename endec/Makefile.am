
ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = endec
endec_SOURCES = endec.c

dist_man_MANS = endec.1

endec.1: endec.c $(top_srcdir)/configure.ac
	which txt2man && ./endec --help | txt2man -d 1 -t "endec" -r "${PACKAGE_NAME}-${PACKAGE_VERSION}" > endec.1 || true


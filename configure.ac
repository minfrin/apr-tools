#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(apr-tools, 0.9.0, minfrin@sharp.fm)
AC_CONFIG_AUX_DIR(build-aux)
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([dist-bzip2])
LT_INIT
AC_CONFIG_FILES([Makefile apr-tools.spec endec/Makefile])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC


# Checks for libraries.
PKG_CHECK_MODULES(apr, apr-1 >= 1.3)
PKG_CHECK_MODULES(apu, apr-util-1 >= 1.3)

CFLAGS="$CFLAGS $apr_CFLAGS $apu_CFLAGS"
CPPFLAGS="$CPPFLAGS $apr_CPPFLAGS $apu_CPPFLAGS"
LDFLAGS="$LDFLAGS $apr_LDFLAGS $apu_LDFLAGS"
LIBS="$LIBS $apr_LIBS $apu_LIBS"


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([getopt])

AC_OUTPUT


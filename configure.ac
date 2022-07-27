#!/usr/bin/autoconf
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(jxtl, 1.0.0, rinehimer@gmail.com)
AC_CONFIG_SRCDIR([libjxtl/utf.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
LT_INIT

# Checks for programs.
AC_PROG_AWK
AC_PROG_YACC
AC_PROG_CC
AC_PROG_LEX

# Checks for libraries.
AC_CHECK_LIB([expat], [XML_ParserCreate])
AC_CHECK_LIB([fl], [yywrap])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stddef.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([strcasecmp strdup])

## Apache portable runtime checking
AC_CHECK_PROGS(APR_CONFIG, apr-1-config apr-config, [/bin/false],
               [$PATH$PATH_SEPARATOR/usr/local/apache2/bin$PATH_SEPARATOR]dnl
               [/usr/apache2/bin])
if test "x$APR_CONFIG" = "x/bin/false" ; then
  AC_MSG_ERROR(Could not find an apr-config, [1])
fi
APR_CFLAGS=`$APR_CONFIG --cflags --cppflags --includes`
APR_LIBS=`$APR_CONFIG --link-ld --libs`
AC_SUBST(APR_CFLAGS)
AC_SUBST(APR_LIBS)

AC_CHECK_PROGS(APU_CONFIG, apu-1-config apu-config, [/bin/false],
               [$PATH$PATH_SEPARATOR/usr/local/apache2/bin$PATH_SEPARATOR]dnl
               [/usr/apache2/bin])
if test "x$APU_CONFIG" = "x/bin/false" ; then
  AC_MSG_ERROR(Could not find an apu-config, [1])
fi

## Don't bother linking against dbm or ldap if our apu-config supports it.
if $APU_CONFIG --avoid-dbm > /dev/null 2>&1 ; then
  AVOID_FLAGS="--avoid-dbm"
fi

if $APU_CONFIG --avoid-ldap > /dev/null 2>&1 ; then
  AVOID_FLAGS="$AVOID_FLAGS --avoid-ldap"
fi

APU_CFLAGS=`$APU_CONFIG --includes`
APU_LIBS=`$APU_CONFIG $AVOID_FLAGS --link-ld --libs`
AC_SUBST(APU_CFLAGS)
AC_SUBST(APU_LIBS)

AC_CHECK_PROG(SWIG, swig, swig, [], [], [])

AC_ARG_ENABLE([static-xml2json],
  AS_HELP_STRING([--enable-static-xml2json], [Build static version of xml2json]),
[case "${enableval}" in
  yes) static_xml2json=true ;;
  no)  static_xml2json=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-static-xml2json]) ;;
esac],[static_xml2json=false])
AM_CONDITIONAL([STATIC_XML2JSON], [test "x${static_xml2json}" = "xtrue"])

AC_CONFIG_FILES([Makefile
                 bindings/perl/Makefile.PL
                 bindings/python/setup.py
                 libjxtl/Makefile
                 src/Makefile
                 test/Makefile])
AC_OUTPUT

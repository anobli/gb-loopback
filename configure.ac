#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([gb-loopback], [0.1], [abailon@baylibre.com])
AC_CONFIG_SRCDIR([gb-loopback.c])
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
AC_SEARCH_LIBS([udev_new], [udev], [], [
  AC_MSG_ERROR([unable to find libudev])
])

# Checks for header files.
AC_CHECK_HEADERS([locale.h stdlib.h unistd.h libudev.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday strrchr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

#
# starter-debugger - configure.ac
#
# Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
# Process this file with autoconf to produce configure.

AC_INIT([starter-debugger], [0.1.0], [startercontroller@gmail.com])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC
AM_PROG_AR

AC_PROG_YACC
AC_PROG_LEX([noyywrap])
# ylwrap expects yacc-style y.tab.*; -Wno-yacc silences %define warnings.
AC_CHECK_PROGS([BISON], [bison])
AS_IF([test -n "$BISON"], [YACC="$BISON -y -Wno-yacc"])

LT_INIT

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.44 gobject-2.0 >= 2.44 gio-2.0 >= 2.44])

AC_CHECK_LIB([readline], [readline], [],
  [AC_MSG_ERROR([libreadline is required])])
AC_CHECK_HEADERS([readline/readline.h readline/history.h], [],
  [AC_MSG_ERROR([readline headers not found])])

ST_DBG_CFLAGS="$GLIB_CFLAGS"
ST_DBG_LIBS="$GLIB_LIBS"

AC_SUBST(ST_DBG_CFLAGS)
AC_SUBST(ST_DBG_LIBS)

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  examples/Makefile
  test/Makefile
])

AC_OUTPUT

#
# starter-debugger - test/Makefile.am
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

AM_CFLAGS = $(ST_DBG_CFLAGS) -I$(top_srcdir)/src -I$(top_srcdir)/test
AM_LDFLAGS = $(ST_DBG_LIBS) -lreadline

TESTS = test-debugger
check_PROGRAMS = $(TESTS)

# Reuse objects built for starter-debugger (session/builtins/parser).
SRC_OBJS = \
	$(top_builddir)/src/starter_debugger-session.o \
	$(top_builddir)/src/starter_debugger-builtins.o \
	$(top_builddir)/src/starter_debugger-cmd-exec.o \
	$(top_builddir)/src/starter_debugger-cmd.o \
	$(top_builddir)/src/starter_debugger-lex.o

test_debugger_SOURCES = test-debugger.c mock-debug-server.c mock-debug-server.h
test_debugger_CFLAGS = $(AM_CFLAGS)
test_debugger_LDADD = $(SRC_OBJS) $(AM_LDFLAGS)
test_debugger_DEPENDENCIES = $(SRC_OBJS)

$(top_builddir)/src/starter_debugger-session.o \
$(top_builddir)/src/starter_debugger-builtins.o \
$(top_builddir)/src/starter_debugger-cmd-exec.o \
$(top_builddir)/src/starter_debugger-cmd.o \
$(top_builddir)/src/starter_debugger-lex.o:
	$(MAKE) -C $(top_builddir)/src

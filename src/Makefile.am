#
# starter-debugger - src/Makefile.am
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

bin_PROGRAMS = starter-debugger

starter_debugger_SOURCES = \
	main.c \
	session.c \
	session.h \
	builtins.c \
	builtins.h \
	cmd-exec.c \
	cmd-exec.h \
	cmd.y \
	lex.l

starter_debugger_CPPFLAGS = $(AM_CPPFLAGS) $(ST_DBG_CFLAGS) -I$(srcdir)
starter_debugger_LDADD = $(ST_DBG_LIBS)
starter_debugger_CFLAGS = $(AM_CFLAGS)

AM_YFLAGS = -d

BUILT_SOURCES = cmd.h
CLEANFILES = cmd.h cmd.c lex.c

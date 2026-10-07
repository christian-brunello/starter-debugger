/*
 * starter-debugger - cmd-exec.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "cmd-exec.h"

#include <string.h>

#include "builtins.h"

/* From flex / bison. */
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string (const char *str);
extern void yy_delete_buffer (YY_BUFFER_STATE buffer);
extern int yyparse (void);
extern STDbgExec *st_dbg_yy_exec;

gboolean
st_dbg_exec_line (STDbgExec * exec, const gchar * line)
{
  YY_BUFFER_STATE buf;
  int rc;

  g_return_val_if_fail (exec != NULL, FALSE);

  if (line == NULL)
    return TRUE;

  while (*line == ' ' || *line == '\t')
    line++;

  if (*line == '\0' || *line == '#')
    return TRUE;

  st_dbg_yy_exec = exec;
  buf = yy_scan_string (line);
  rc = yyparse ();
  yy_delete_buffer (buf);
  st_dbg_yy_exec = NULL;

  return rc == 0;
}

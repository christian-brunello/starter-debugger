/*
 * starter-debugger - cmd-exec.h
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

#ifndef ST_DBG_CMD_EXEC_H_INCLUDED
#define ST_DBG_CMD_EXEC_H_INCLUDED

#include <glib.h>

#include "session.h"

G_BEGIN_DECLS

typedef struct _STDbgExec STDbgExec;

struct _STDbgExec
{
  STDbgSession *session;
  gboolean quit;
  guint source_depth;
};

/* Parse and run one line (or a whole script via source). */
gboolean st_dbg_exec_line (STDbgExec * exec, const gchar * line);

G_END_DECLS
#endif /* ST_DBG_CMD_EXEC_H_INCLUDED */

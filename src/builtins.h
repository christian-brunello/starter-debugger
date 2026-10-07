/*
 * starter-debugger - builtins.h
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

#ifndef ST_DBG_BUILTINS_H_INCLUDED
#define ST_DBG_BUILTINS_H_INCLUDED

#include "cmd-exec.h"

G_BEGIN_DECLS

void st_dbg_cmd_help (STDbgExec * exec);
void st_dbg_cmd_quit (STDbgExec * exec);
void st_dbg_cmd_status (STDbgExec * exec);
void st_dbg_cmd_source (STDbgExec * exec, const gchar * path);
void st_dbg_cmd_info_peers (STDbgExec * exec);
void st_dbg_cmd_info_io (STDbgExec * exec, const gchar * filter);
void st_dbg_cmd_info_vars (STDbgExec * exec, const gchar * filter);
void st_dbg_cmd_info_breakpoints (STDbgExec * exec);
void st_dbg_cmd_info_watchpoints (STDbgExec * exec);
/* channel NULL + mode NULL → show; mode on/off/all; channel name ± on/off. */
void st_dbg_cmd_monitor (STDbgExec * exec, const gchar * channel,
			 const gchar * mode);

void st_dbg_cmd_break (STDbgExec * exec, const gchar * id);
void st_dbg_cmd_watch (STDbgExec * exec, const gchar * id);
void st_dbg_cmd_delete_break (STDbgExec * exec, guint handle);
void st_dbg_cmd_delete_watch (STDbgExec * exec, guint handle);
void st_dbg_cmd_continue (STDbgExec * exec);
void st_dbg_cmd_set_input (STDbgExec * exec, const gchar * id, gdouble value);
void st_dbg_cmd_set_output (STDbgExec * exec, const gchar * id,
			    gdouble value);

G_END_DECLS
#endif /* ST_DBG_BUILTINS_H_INCLUDED */

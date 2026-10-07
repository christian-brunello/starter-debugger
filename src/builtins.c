/*
 * starter-debugger - builtins.c
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

#define _GNU_SOURCE

#include "builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ST_DBG_MAX_SOURCE_DEPTH 32

void
st_dbg_cmd_help (STDbgExec * exec)
{
  (void) exec;
  g_print ("List of commands:\n");
  g_print ("\n");
  g_print ("help, h                  -- show this help\n");
  g_print ("status                   -- call GetStatus on the debug server\n");
  g_print ("info peers               -- list peers (ListPeers)\n");
  g_print ("info io [FILTER]         -- list inputs/outputs (snapshot)\n");
  g_print ("info vars [FILTER]       -- shadowed variables\n");
  g_print ("info break               -- list breakpoints\n");
  g_print ("info watch               -- list watchpoints\n");
  g_print ("monitor [all|on|off]     -- live signals (all channels)\n");
  g_print ("monitor CHANNEL [on|off] -- inputs|outputs|io|stats|vars|peers|stopped\n");
  g_print ("break, b ID              -- breakpoint on identifier\n");
  g_print ("watch, w ID              -- watchpoint on value change\n");
  g_print ("delete break HANDLE      -- remove breakpoint\n");
  g_print ("delete watch HANDLE      -- remove watchpoint\n");
  g_print ("continue, c              -- resume after Stopped\n");
  g_print ("set input ID = VALUE     -- SetInput poke\n");
  g_print ("set output ID = VALUE    -- SetOutput poke\n");
  g_print ("source FILE              -- run commands from FILE\n");
  g_print ("quit, exit, q            -- leave the debugger\n");
  g_print ("\n");
  g_print ("Lines starting with # are comments.\n");
  g_print ("Break: pause before engine_apply for ID.\n");
  g_print ("Watch: pause when ID's value changes.\n");
}

void
st_dbg_cmd_quit (STDbgExec * exec)
{
  g_return_if_fail (exec != NULL);
  exec->quit = TRUE;
}

void
st_dbg_cmd_status (STDbgExec * exec)
{
  GError *error = NULL;
  gboolean paused = FALSE;

  g_return_if_fail (exec != NULL);

  if (!st_dbg_session_print_status (exec->session, &error))
    {
      g_printerr ("status: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
      return;
    }

  if (st_dbg_session_get_paused (exec->session, &paused, &error))
    g_print ("  paused (live): %s\n", paused ? "true" : "false");
  else
    g_clear_error (&error);
}

void
st_dbg_cmd_info_peers (STDbgExec * exec)
{
  GError *error = NULL;

  g_return_if_fail (exec != NULL);

  if (!st_dbg_session_print_peers (exec->session, &error))
    {
      g_printerr ("info peers: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
    }
}

void
st_dbg_cmd_info_io (STDbgExec * exec, const gchar * filter)
{
  GError *error = NULL;

  g_return_if_fail (exec != NULL);
  if (!st_dbg_session_print_io (exec->session, filter, &error))
    {
      g_printerr ("info io: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
    }
}

void
st_dbg_cmd_info_vars (STDbgExec * exec, const gchar * filter)
{
  g_return_if_fail (exec != NULL);
  st_dbg_session_print_vars (exec->session, filter);
}

void
st_dbg_cmd_info_breakpoints (STDbgExec * exec)
{
  GError *error = NULL;

  g_return_if_fail (exec != NULL);
  if (!st_dbg_session_print_breakpoints (exec->session, &error))
    {
      g_printerr ("info break: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
    }
}

void
st_dbg_cmd_info_watchpoints (STDbgExec * exec)
{
  GError *error = NULL;

  g_return_if_fail (exec != NULL);
  if (!st_dbg_session_print_watchpoints (exec->session, &error))
    {
      g_printerr ("info watch: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
    }
}

void
st_dbg_cmd_monitor (STDbgExec * exec, const gchar * channel,
		    const gchar * mode)
{
  guint bits = ST_DBG_MON_NONE;
  gboolean enable;

  g_return_if_fail (exec != NULL);

  /* monitor */
  if ((channel == NULL || *channel == '\0')
      && (mode == NULL || *mode == '\0'))
    {
      st_dbg_session_print_monitor (exec->session);
      return;
    }

  /* monitor on|off|all */
  if (channel == NULL || *channel == '\0')
    {
      if (g_ascii_strcasecmp (mode, "on") == 0
	  || g_ascii_strcasecmp (mode, "all") == 0)
	st_dbg_session_set_monitor (exec->session, TRUE);
      else if (g_ascii_strcasecmp (mode, "off") == 0)
	st_dbg_session_set_monitor (exec->session, FALSE);
      else
	g_printerr ("monitor: use on|off|all or CHANNEL [on|off]\n");
      return;
    }

  if (!st_dbg_session_parse_monitor_channel (channel, &bits))
    {
      g_printerr ("monitor: unknown channel '%s'\n", channel);
      g_printerr ("  channels: all inputs outputs io stats vars peers stopped\n");
      return;
    }

  /* monitor CHANNEL  → enable; monitor CHANNEL on|off */
  if (mode == NULL || *mode == '\0')
    {
      if (bits == ST_DBG_MON_ALL)
	st_dbg_session_set_monitor_mask (exec->session, ST_DBG_MON_ALL);
      else
	st_dbg_session_monitor_set_channel (exec->session, bits, TRUE);
      return;
    }

  if (g_ascii_strcasecmp (mode, "on") == 0)
    enable = TRUE;
  else if (g_ascii_strcasecmp (mode, "off") == 0)
    enable = FALSE;
  else
    {
      g_printerr ("monitor: use on or off after channel\n");
      return;
    }

  if (bits == ST_DBG_MON_ALL)
    st_dbg_session_set_monitor (exec->session, enable);
  else
    st_dbg_session_monitor_set_channel (exec->session, bits, enable);
}

void
st_dbg_cmd_break (STDbgExec * exec, const gchar * id)
{
  GError *error = NULL;
  guint handle = 0;

  g_return_if_fail (exec != NULL);
  g_return_if_fail (id != NULL);

  if (!st_dbg_session_add_breakpoint (exec->session, id, &handle, &error))
    {
      g_printerr ("break: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
      return;
    }
  g_print ("Breakpoint %u on %s\n", handle, id);
}

void
st_dbg_cmd_watch (STDbgExec * exec, const gchar * id)
{
  GError *error = NULL;
  guint handle = 0;

  g_return_if_fail (exec != NULL);
  g_return_if_fail (id != NULL);

  if (!st_dbg_session_add_watchpoint (exec->session, id, &handle, &error))
    {
      g_printerr ("watch: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
      return;
    }
  g_print ("Watchpoint %u on %s\n", handle, id);
}

void
st_dbg_cmd_delete_break (STDbgExec * exec, guint handle)
{
  GError *error = NULL;

  g_return_if_fail (exec != NULL);

  if (!st_dbg_session_remove_breakpoint (exec->session, handle, &error))
    {
      g_printerr ("delete break: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
      return;
    }
  g_print ("Deleted breakpoint %u\n", handle);
}

void
st_dbg_cmd_delete_watch (STDbgExec * exec, guint handle)
{
  GError *error = NULL;

  g_return_if_fail (exec != NULL);

  if (!st_dbg_session_remove_watchpoint (exec->session, handle, &error))
    {
      g_printerr ("delete watch: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
      return;
    }
  g_print ("Deleted watchpoint %u\n", handle);
}

void
st_dbg_cmd_continue (STDbgExec * exec)
{
  GError *error = NULL;

  g_return_if_fail (exec != NULL);

  if (!st_dbg_session_continue (exec->session, &error))
    {
      g_printerr ("continue: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
      return;
    }
  g_print ("Continuing.\n");
}

void
st_dbg_cmd_set_input (STDbgExec * exec, const gchar * id, gdouble value)
{
  GError *error = NULL;

  g_return_if_fail (exec != NULL);
  g_return_if_fail (id != NULL);

  if (!st_dbg_session_set_input (exec->session, id, value, 0, &error))
    {
      g_printerr ("set input: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
      return;
    }
  g_print ("SetInput %s = %g\n", id, value);
}

void
st_dbg_cmd_set_output (STDbgExec * exec, const gchar * id, gdouble value)
{
  GError *error = NULL;

  g_return_if_fail (exec != NULL);
  g_return_if_fail (id != NULL);

  if (!st_dbg_session_set_output (exec->session, id, value, 0, &error))
    {
      g_printerr ("set output: %s\n", error ? error->message : "failed");
      g_clear_error (&error);
      return;
    }
  g_print ("SetOutput %s = %g\n", id, value);
}

void
st_dbg_cmd_source (STDbgExec * exec, const gchar * path)
{
  FILE *fp;
  char *line = NULL;
  size_t cap = 0;
  ssize_t n;
  guint lineno = 0;

  g_return_if_fail (exec != NULL);
  g_return_if_fail (path != NULL);

  if (exec->source_depth >= ST_DBG_MAX_SOURCE_DEPTH)
    {
      g_printerr ("source: nesting too deep (max %d)\n",
		  ST_DBG_MAX_SOURCE_DEPTH);
      return;
    }

  fp = fopen (path, "r");
  if (fp == NULL)
    {
      g_printerr ("source: cannot open '%s'\n", path);
      return;
    }

  exec->source_depth++;
  while (!exec->quit && (n = getline (&line, &cap, fp)) != -1)
    {
      lineno++;
      while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
	line[--n] = '\0';

      if (!st_dbg_exec_line (exec, line))
	g_printerr ("source: %s:%u: parse error\n", path, lineno);
    }

  free (line);
  fclose (fp);
  exec->source_depth--;
}

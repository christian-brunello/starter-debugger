/*
 * starter-debugger - main.c
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "builtins.h"
#include "cmd-exec.h"
#include "session.h"

#define ST_DBG_HISTORY_FILE ".starter-debugger_history"
#define ST_DBG_PROMPT "(starter) "

static GMainLoop *repl_loop = NULL;
static STDbgExec *repl_exec = NULL;

static gchar *
history_path (void)
{
  const gchar *home = g_get_home_dir ();

  if (home == NULL)
    return NULL;
  return g_build_filename (home, ST_DBG_HISTORY_FILE, NULL);
}

static void
load_history (void)
{
  g_autofree gchar *path = history_path ();

  if (path != NULL)
    read_history (path);
}

static void
save_history (void)
{
  g_autofree gchar *path = history_path ();

  if (path != NULL)
    write_history (path);
}

static void
async_print_begin (gpointer user_data)
{
  (void) user_data;
  g_print ("\n");
}

static void
async_print_end (gpointer user_data)
{
  (void) user_data;
  rl_on_new_line ();
  rl_redisplay ();
}

static void
on_readline_line (char *line)
{
  if (line == NULL)
    {
      g_print ("\n");
      if (repl_loop)
	g_main_loop_quit (repl_loop);
      return;
    }

  if (*line != '\0')
    add_history (line);

  st_dbg_exec_line (repl_exec, line);
  free (line);

  if (repl_exec->quit && repl_loop)
    g_main_loop_quit (repl_loop);
}

static gboolean
on_stdin_readable (GIOChannel * source, GIOCondition condition,
		   gpointer data)
{
  (void) source;
  (void) data;

  if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL))
    {
      if (repl_loop)
	g_main_loop_quit (repl_loop);
      return FALSE;
    }

  rl_callback_read_char ();
  return TRUE;
}

/* Read and execute available stdin lines. Returns FALSE on EOF/error. */
static gboolean
pipe_stdin_drain (GIOChannel * source)
{
  for (;;)
    {
      GIOStatus st;
      gchar *line = NULL;
      gsize len = 0;
      GError *err = NULL;

      st = g_io_channel_read_line (source, &line, &len, NULL, &err);
      if (st == G_IO_STATUS_AGAIN)
	{
	  g_clear_error (&err);
	  return TRUE;
	}
      if (st == G_IO_STATUS_EOF)
	{
	  g_clear_error (&err);
	  if (repl_loop)
	    g_main_loop_quit (repl_loop);
	  return FALSE;
	}
      if (st != G_IO_STATUS_NORMAL)
	{
	  g_clear_error (&err);
	  if (repl_loop)
	    g_main_loop_quit (repl_loop);
	  return FALSE;
	}

      while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
	line[--len] = '\0';

      st_dbg_exec_line (repl_exec, line);
      g_free (line);

      if (repl_exec->quit)
	{
	  if (repl_loop)
	    g_main_loop_quit (repl_loop);
	  return FALSE;
	}
    }
}

static gboolean
on_pipe_stdin (GIOChannel * source, GIOCondition condition, gpointer data)
{
  (void) data;

  if (condition & (G_IO_ERR | G_IO_NVAL))
    {
      if (repl_loop)
	g_main_loop_quit (repl_loop);
      return FALSE;
    }

  if (!pipe_stdin_drain (source))
    return FALSE;

  /* HUP with no more data: end of script. */
  if (condition & G_IO_HUP)
    {
      if (repl_loop)
	g_main_loop_quit (repl_loop);
      return FALSE;
    }

  return TRUE;
}

static void
repl (STDbgExec * exec)
{
  g_autoptr (GIOChannel) channel = NULL;
  guint watch_id;

  repl_exec = exec;
  repl_loop = g_main_loop_new (NULL, FALSE);

  st_dbg_session_set_async_print_hooks (exec->session, async_print_begin,
					async_print_end, NULL);

  rl_callback_handler_install (ST_DBG_PROMPT, on_readline_line);
  channel = g_io_channel_unix_new (STDIN_FILENO);
  g_io_channel_set_encoding (channel, NULL, NULL);
  g_io_channel_set_buffered (channel, FALSE);
  watch_id =
    g_io_add_watch (channel, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
		    on_stdin_readable, NULL);

  g_main_loop_run (repl_loop);

  g_source_remove (watch_id);
  rl_callback_handler_remove ();
  st_dbg_session_set_async_print_hooks (exec->session, NULL, NULL, NULL);
  g_clear_pointer (&repl_loop, g_main_loop_unref);
  repl_exec = NULL;
}

int
main (int argc, char **argv)
{
  g_autofree gchar *core_spec = NULL;
  g_autofree gchar *host = NULL;
  gchar *script_path = NULL;
  gboolean show_version = FALSE;
  guint16 port = ST_DBG_DEFAULT_PORT;
  GError *error = NULL;
  STDbgSession *session;
  STDbgExec exec;
  GOptionContext *context;
  GOptionEntry entries[] = {
    {"core", 'c', 0, G_OPTION_ARG_STRING, &core_spec,
     "Core debug server address (default: " ST_DBG_DEFAULT_HOST
     ":17373)", "HOST:PORT"},
    {"version", 'v', 0, G_OPTION_ARG_NONE, &show_version,
     "Print version and exit", NULL},
    {0}
  };

  context = g_option_context_new ("[SCRIPT]");
  g_option_context_set_summary (context,
				"STARTER debugger - attach to starter-core debug server");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      g_option_context_free (context);
      return 1;
    }
  g_option_context_free (context);

  if (show_version)
    {
      g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
      return 0;
    }

  if (argc > 2)
    {
      g_printerr ("Usage: %s [--core=HOST:PORT] [SCRIPT]\n", argv[0]);
      return 1;
    }
  if (argc == 2)
    script_path = argv[1];

  if (core_spec == NULL)
    core_spec =
      g_strdup_printf ("%s:%u", ST_DBG_DEFAULT_HOST,
		       (unsigned) ST_DBG_DEFAULT_PORT);

  if (!st_dbg_session_parse_bind (core_spec, &host, &port, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      return 1;
    }

  session = st_dbg_session_new ();
  g_print ("Connecting to %s:%u...\n", host, (unsigned) port);
  if (!st_dbg_session_connect (session, host, port, &error))
    {
      g_printerr ("connect: %s\n", error->message);
      g_clear_error (&error);
      st_dbg_session_free (session);
      return 1;
    }

  memset (&exec, 0, sizeof (exec));
  exec.session = session;

  if (!st_dbg_session_print_status (session, &error))
    {
      g_printerr ("GetStatus: %s\n", error->message);
      g_clear_error (&error);
      st_dbg_session_free (session);
      return 1;
    }

  if (script_path != NULL)
    {
      st_dbg_cmd_source (&exec, script_path);
      st_dbg_session_free (session);
      return 0;
    }

  if (isatty (STDIN_FILENO))
    {
      load_history ();
      using_history ();
      repl (&exec);
      save_history ();
    }
  else
    {
      /* Non-interactive: GMainLoop so D-Bus signals run while waiting on stdin. */
      g_autoptr (GIOChannel) channel = NULL;
      g_autoptr (GMainLoop) loop = NULL;
      guint watch_id;

      repl_exec = &exec;
      loop = g_main_loop_new (NULL, FALSE);
      repl_loop = loop;

      channel = g_io_channel_unix_new (STDIN_FILENO);
      g_io_channel_set_encoding (channel, NULL, NULL);
      g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, NULL);
      watch_id =
	g_io_add_watch (channel, G_IO_IN | G_IO_HUP | G_IO_ERR, on_pipe_stdin,
			NULL);
      /* Data may already be buffered before the watch was installed. */
      pipe_stdin_drain (channel);
      if (!exec.quit)
	g_main_loop_run (loop);
      if (watch_id > 0 && g_main_context_find_source_by_id (NULL, watch_id))
	g_source_remove (watch_id);

      repl_loop = NULL;
      repl_exec = NULL;
    }

  st_dbg_session_free (session);
  return 0;
}

/*
 * starter-debugger - test-debugger.c
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

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "session.h"
#include "cmd-exec.h"
#include "mock-debug-server.h"

typedef struct
{
  STDbgMockServer *mock;
  STDbgSession *session;
} Fixture;

static void
wait_ms (guint ms)
{
  g_usleep (ms * 1000u);
  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);
}

static void
fixture_setup (Fixture * f, gconstpointer user_data)
{
  GError *error = NULL;

  (void) user_data;
  memset (f, 0, sizeof (*f));

  f->mock = st_dbg_mock_server_start (&error);
  g_assert_no_error (error);
  g_assert_nonnull (f->mock);
  g_assert_cmpuint (st_dbg_mock_server_get_port (f->mock), >, 0);

  f->session = st_dbg_session_new ();
  g_assert_true (st_dbg_session_connect
		 (f->session, st_dbg_mock_server_get_host (f->mock),
		  st_dbg_mock_server_get_port (f->mock), &error));
  g_assert_no_error (error);
  g_assert_true (st_dbg_session_is_connected (f->session));

  /* Let the mock register the object on the new connection. */
  wait_ms (50);
}

static void
fixture_teardown (Fixture * f, gconstpointer user_data)
{
  (void) user_data;
  /* Close the client first so the mock can handle "closed" while alive. */
  g_clear_pointer (&f->session, st_dbg_session_free);
  wait_ms (30);
  g_clear_pointer (&f->mock, st_dbg_mock_server_stop);
}

static void
test_parse_bind (void)
{
  g_autofree gchar *host = NULL;
  guint16 port = 0;
  GError *error = NULL;

  g_assert_true (st_dbg_session_parse_bind ("127.0.0.1:17373", &host, &port,
					    &error));
  g_assert_no_error (error);
  g_assert_cmpstr (host, ==, "127.0.0.1");
  g_assert_cmpuint (port, ==, 17373);
  g_clear_pointer (&host, g_free);

  g_assert_false (st_dbg_session_parse_bind ("no-port", &host, &port, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
}

static void
test_get_status (Fixture * f, gconstpointer user_data)
{
  GError *error = NULL;

  (void) user_data;
  g_assert_true (st_dbg_session_print_status (f->session, &error));
  g_assert_no_error (error);
}

static void
test_list_peers (Fixture * f, gconstpointer user_data)
{
  GError *error = NULL;

  (void) user_data;
  st_dbg_mock_server_add_peer (f->mock, "ST:Mock", "10.0.0.1", 1234, 2);
  wait_ms (50);

  g_assert_true (st_dbg_session_print_peers (f->session, &error));
  g_assert_no_error (error);
}

static void
test_break_watch_continue (Fixture * f, gconstpointer user_data)
{
  GError *error = NULL;
  guint bhandle = 0, whandle = 0;
  gboolean paused = FALSE;

  (void) user_data;

  g_assert_true (st_dbg_session_add_breakpoint
		 (f->session, "sensor:temp", &bhandle, &error));
  g_assert_no_error (error);
  g_assert_cmpuint (bhandle, >, 0);
  g_assert_cmpuint (st_dbg_mock_server_n_breakpoints (f->mock), ==, 1);

  g_assert_true (st_dbg_session_add_watchpoint
		 (f->session, "sensor:temp", &whandle, &error));
  g_assert_no_error (error);
  g_assert_cmpuint (whandle, >, 0);
  g_assert_cmpuint (st_dbg_mock_server_n_watchpoints (f->mock), ==, 1);

  g_assert_true (st_dbg_session_print_breakpoints (f->session, &error));
  g_assert_true (st_dbg_session_print_watchpoints (f->session, &error));

  st_dbg_mock_server_emit_stopped (f->mock, "watchpoint", "sensor:temp", 21.5,
				   whandle);
  wait_ms (50);
  g_assert_true (st_dbg_mock_server_is_paused (f->mock));

  g_assert_true (st_dbg_session_get_paused (f->session, &paused, &error));
  g_assert_no_error (error);
  g_assert_true (paused);

  g_assert_true (st_dbg_session_continue (f->session, &error));
  g_assert_no_error (error);
  wait_ms (20);
  g_assert_false (st_dbg_mock_server_is_paused (f->mock));

  g_assert_true (st_dbg_session_remove_breakpoint
		 (f->session, bhandle, &error));
  g_assert_true (st_dbg_session_remove_watchpoint
		 (f->session, whandle, &error));
  g_assert_cmpuint (st_dbg_mock_server_n_breakpoints (f->mock), ==, 0);
  g_assert_cmpuint (st_dbg_mock_server_n_watchpoints (f->mock), ==, 0);
}

static void
test_set_input_output (Fixture * f, gconstpointer user_data)
{
  GError *error = NULL;
  g_autofree gchar *id = NULL;
  gdouble value = 0;

  (void) user_data;

  g_assert_true (st_dbg_session_set_input
		 (f->session, "in:foo", 3.14, 0, &error));
  g_assert_no_error (error);
  g_assert_true (st_dbg_mock_server_last_set_input (f->mock, &id, &value));
  g_assert_cmpstr (id, ==, "in:foo");
  g_assert_cmpfloat (value, ==, 3.14);
  g_clear_pointer (&id, g_free);

  g_assert_true (st_dbg_session_set_output
		 (f->session, "out:bar", 7.0, 0, &error));
  g_assert_no_error (error);
  g_assert_true (st_dbg_mock_server_last_set_output (f->mock, &id, &value));
  g_assert_cmpstr (id, ==, "out:bar");
  g_assert_cmpfloat (value, ==, 7.0);
}

static void
test_input_signal_shadow (Fixture * f, gconstpointer user_data)
{
  GError *error = NULL;

  (void) user_data;

  st_dbg_session_set_monitor (f->session, FALSE);
  st_dbg_mock_server_emit_input (f->mock, "sensor:temp", 42.0);
  wait_ms (80);

  /* ListInputs includes the emitted input; filter should match. */
  g_assert_true (st_dbg_session_print_io (f->session, "sensor", &error));
  g_assert_no_error (error);
}

static void
test_list_io_snapshot (Fixture * f, gconstpointer user_data)
{
  GError *error = NULL;

  (void) user_data;

  /* Stable I/O never seen via signals must still appear after List*. */
  st_dbg_mock_server_set_output (f->mock, "CLK:ChFlowSetpointTemp",
				 "boiler setpoint", 45.0);
  st_dbg_mock_server_set_input (f->mock, "CLK:OutsideTemp", "outside", 12.5);
  wait_ms (50);

  g_assert_true (st_dbg_session_print_io
		 (f->session, "ChFlowSetpoint", &error));
  g_assert_no_error (error);
  g_assert_true (st_dbg_session_print_io (f->session, "Outside", &error));
  g_assert_no_error (error);
}

static void
test_monitor_rules_triggers (Fixture * f, gconstpointer user_data)
{
  STDbgExec exec;

  (void) user_data;
  memset (&exec, 0, sizeof (exec));
  exec.session = f->session;

  /* Enable only rules: emit must not crash; channel filters apply. */
  g_assert_true (st_dbg_exec_line (&exec, "monitor rules"));
  st_dbg_mock_server_emit_rule_triggered (f->mock, "beep:status",
					  "SET 1 WHEN (time:hour) == 7");
  st_dbg_mock_server_emit_trigger_triggered (f->mock, "alarm_active",
					     "TRIGGER WHEN x SET y = 1");
  wait_ms (80);

  g_assert_true (st_dbg_exec_line (&exec, "monitor triggers"));
  g_assert_cmpuint (st_dbg_session_get_monitor_mask (f->session), ==,
		    ST_DBG_MON_RULES | ST_DBG_MON_TRIGGERS);
  st_dbg_mock_server_emit_trigger_triggered (f->mock, "alarm_active",
					     "TRIGGER WHEN x SET y = 1");
  wait_ms (50);
  g_assert_true (st_dbg_exec_line (&exec, "monitor off"));
}

static void
test_commands_cli (Fixture * f, gconstpointer user_data)
{
  STDbgExec exec;
  GError *error = NULL;
  gboolean paused = FALSE;

  (void) user_data;
  memset (&exec, 0, sizeof (exec));
  exec.session = f->session;

  g_assert_true (st_dbg_exec_line (&exec, "# comment only"));
  g_assert_true (st_dbg_exec_line (&exec, ""));
  g_assert_true (st_dbg_exec_line (&exec, "help"));
  g_assert_true (st_dbg_exec_line (&exec, "status"));
  g_assert_true (st_dbg_exec_line (&exec, "info peers"));
  g_assert_true (st_dbg_exec_line (&exec, "info break"));
  g_assert_true (st_dbg_exec_line (&exec, "info watch"));
  g_assert_true (st_dbg_exec_line (&exec, "monitor on"));
  g_assert_true (st_dbg_session_get_monitor (f->session));
  g_assert_cmpuint (st_dbg_session_get_monitor_mask (f->session), ==,
		    ST_DBG_MON_ALL);
  g_assert_true (st_dbg_exec_line (&exec, "monitor off"));
  g_assert_false (st_dbg_session_get_monitor (f->session));
  g_assert_true (st_dbg_exec_line (&exec, "monitor inputs"));
  g_assert_cmpuint (st_dbg_session_get_monitor_mask (f->session), ==,
		    ST_DBG_MON_INPUTS);
  g_assert_true (st_dbg_exec_line (&exec, "monitor stats on"));
  g_assert_cmpuint (st_dbg_session_get_monitor_mask (f->session), ==,
		    ST_DBG_MON_INPUTS | ST_DBG_MON_STATS);
  g_assert_true (st_dbg_exec_line (&exec, "monitor inputs off"));
  g_assert_cmpuint (st_dbg_session_get_monitor_mask (f->session), ==,
		    ST_DBG_MON_STATS);
  g_assert_true (st_dbg_exec_line (&exec, "monitor all"));
  g_assert_cmpuint (st_dbg_session_get_monitor_mask (f->session), ==,
		    ST_DBG_MON_ALL);
  g_assert_true (st_dbg_exec_line (&exec, "monitor off"));
  g_assert_true (st_dbg_exec_line (&exec, "monitor rules"));
  g_assert_cmpuint (st_dbg_session_get_monitor_mask (f->session), ==,
		    ST_DBG_MON_RULES);
  g_assert_true (st_dbg_exec_line (&exec, "monitor triggers on"));
  g_assert_cmpuint (st_dbg_session_get_monitor_mask (f->session), ==,
		    ST_DBG_MON_RULES | ST_DBG_MON_TRIGGERS);
  g_assert_true (st_dbg_exec_line (&exec, "monitor off"));
  g_assert_true (st_dbg_exec_line (&exec, "monitor engine"));
  g_assert_cmpuint (st_dbg_session_get_monitor_mask (f->session), ==,
		    ST_DBG_MON_RULES | ST_DBG_MON_TRIGGERS);
  g_assert_true (st_dbg_exec_line (&exec, "monitor off"));

  g_assert_true (st_dbg_exec_line (&exec, "break sensor:temp"));
  g_assert_cmpuint (st_dbg_mock_server_n_breakpoints (f->mock), ==, 1);
  g_assert_true (st_dbg_exec_line (&exec, "watch sensor:temp"));
  g_assert_cmpuint (st_dbg_mock_server_n_watchpoints (f->mock), ==, 1);
  g_assert_true (st_dbg_exec_line (&exec, "info break"));
  g_assert_true (st_dbg_exec_line (&exec, "info watch"));

  st_dbg_mock_server_emit_stopped (f->mock, "breakpoint", "sensor:temp", 1.0,
				   1);
  wait_ms (50);
  g_assert_true (st_dbg_exec_line (&exec, "continue"));
  wait_ms (20);
  g_assert_true (st_dbg_session_get_paused (f->session, &paused, &error));
  g_assert_no_error (error);
  g_assert_false (paused);

  g_assert_true (st_dbg_exec_line (&exec, "delete break 1"));
  g_assert_true (st_dbg_exec_line (&exec, "delete watch 2"));
  g_assert_cmpuint (st_dbg_mock_server_n_breakpoints (f->mock), ==, 0);
  g_assert_cmpuint (st_dbg_mock_server_n_watchpoints (f->mock), ==, 0);

  g_assert_true (st_dbg_exec_line (&exec, "set input in:x = 1.5"));
  g_assert_true (st_dbg_exec_line (&exec, "set output out:y = 9"));

  g_assert_false (st_dbg_exec_line (&exec, "notacommand"));
  g_assert_false (exec.quit);
  g_assert_true (st_dbg_exec_line (&exec, "quit"));
  g_assert_true (exec.quit);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/debugger/parse-bind", test_parse_bind);

  g_test_add ("/debugger/get-status", Fixture, NULL, fixture_setup,
	      test_get_status, fixture_teardown);
  g_test_add ("/debugger/list-peers", Fixture, NULL, fixture_setup,
	      test_list_peers, fixture_teardown);
  g_test_add ("/debugger/break-watch-continue", Fixture, NULL, fixture_setup,
	      test_break_watch_continue, fixture_teardown);
  g_test_add ("/debugger/set-input-output", Fixture, NULL, fixture_setup,
	      test_set_input_output, fixture_teardown);
  g_test_add ("/debugger/input-signal-shadow", Fixture, NULL, fixture_setup,
	      test_input_signal_shadow, fixture_teardown);
  g_test_add ("/debugger/list-io-snapshot", Fixture, NULL, fixture_setup,
	      test_list_io_snapshot, fixture_teardown);
  g_test_add ("/debugger/monitor-rules-triggers", Fixture, NULL, fixture_setup,
	      test_monitor_rules_triggers, fixture_teardown);
  g_test_add ("/debugger/commands-cli", Fixture, NULL, fixture_setup,
	      test_commands_cli, fixture_teardown);

  return g_test_run ();
}

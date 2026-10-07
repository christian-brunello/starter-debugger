/*
 * starter-debugger - session.h
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

#ifndef ST_DBG_SESSION_H_INCLUDED
#define ST_DBG_SESSION_H_INCLUDED

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define ST_DBG_DEFAULT_HOST "127.0.0.1"
#define ST_DBG_DEFAULT_PORT 17373

#define ST_DBG_OBJECT_PATH "/org/starter/core/debug"
#define ST_DBG_INTERFACE   "org.starter.Core.Debug"

typedef struct _STDbgSession STDbgSession;

/* Optional hooks so monitor lines play nice with readline. */
typedef void (*STDbgAsyncPrintFunc) (gpointer user_data);

STDbgSession *st_dbg_session_new (void);
void st_dbg_session_free (STDbgSession * self);

void st_dbg_session_set_async_print_hooks (STDbgSession * self,
					   STDbgAsyncPrintFunc begin,
					   STDbgAsyncPrintFunc end,
					   gpointer user_data);

gboolean st_dbg_session_parse_bind (const gchar * spec, gchar ** host,
				    guint16 * port, GError ** error);

gboolean st_dbg_session_connect (STDbgSession * self, const gchar * host,
				 guint16 port, GError ** error);
void st_dbg_session_disconnect (STDbgSession * self);

gboolean st_dbg_session_is_connected (STDbgSession * self);
const gchar *st_dbg_session_get_host (STDbgSession * self);
guint16 st_dbg_session_get_port (STDbgSession * self);
GDBusConnection *st_dbg_session_get_connection (STDbgSession * self);

gboolean st_dbg_session_print_status (STDbgSession * self, GError ** error);

/* Refresh peers via ListPeers, then print. */
gboolean st_dbg_session_print_peers (STDbgSession * self, GError ** error);

/*
 * Refresh I/O via ListInputs/ListOutputs (falls back to signal shadow on
 * failure), then print. Optional @filter: substring match on id (NULL = all).
 */
gboolean st_dbg_session_print_io (STDbgSession * self, const gchar * filter,
				  GError ** error);
/* Print shadowed vars from VarChanged since attach. */
void st_dbg_session_print_vars (STDbgSession * self, const gchar * filter);

/* Monitor channels (live signal printing). Shadow tables always update. */
typedef enum
{
  ST_DBG_MON_NONE = 0,
  ST_DBG_MON_INPUTS = 1 << 0,
  ST_DBG_MON_OUTPUTS = 1 << 1,
  ST_DBG_MON_STATS = 1 << 2,
  ST_DBG_MON_VARS = 1 << 3,
  ST_DBG_MON_PEERS = 1 << 4,
  ST_DBG_MON_STOPPED = 1 << 5,
  ST_DBG_MON_ALL = (ST_DBG_MON_INPUTS | ST_DBG_MON_OUTPUTS | ST_DBG_MON_STATS
		    | ST_DBG_MON_VARS | ST_DBG_MON_PEERS | ST_DBG_MON_STOPPED)
} STDbgMonitorMask;

void st_dbg_session_set_monitor (STDbgSession * self, gboolean on);
gboolean st_dbg_session_get_monitor (STDbgSession * self);
void st_dbg_session_set_monitor_mask (STDbgSession * self, guint mask);
guint st_dbg_session_get_monitor_mask (STDbgSession * self);
void st_dbg_session_monitor_set_channel (STDbgSession * self, guint bits,
					 gboolean enable);
void st_dbg_session_print_monitor (STDbgSession * self);
/* Parse channel name -> bits. Accepts all/inputs/input/io/outputs/... */
gboolean st_dbg_session_parse_monitor_channel (const gchar * name,
					       guint * bits_out);

/* Breakpoints / watchpoints / continue / poke. */
gboolean st_dbg_session_add_breakpoint (STDbgSession * self, const gchar * id,
					guint * handle_out, GError ** error);
gboolean st_dbg_session_remove_breakpoint (STDbgSession * self, guint handle,
					   GError ** error);
gboolean st_dbg_session_print_breakpoints (STDbgSession * self,
					   GError ** error);

gboolean st_dbg_session_add_watchpoint (STDbgSession * self, const gchar * id,
					guint * handle_out, GError ** error);
gboolean st_dbg_session_remove_watchpoint (STDbgSession * self, guint handle,
					   GError ** error);
gboolean st_dbg_session_print_watchpoints (STDbgSession * self,
					   GError ** error);

gboolean st_dbg_session_continue (STDbgSession * self, GError ** error);
gboolean st_dbg_session_get_paused (STDbgSession * self, gboolean * paused,
				    GError ** error);

gboolean st_dbg_session_set_input (STDbgSession * self, const gchar * id,
				   gdouble value, guint64 flags,
				   GError ** error);
gboolean st_dbg_session_set_output (STDbgSession * self, const gchar * id,
				    gdouble value, guint64 flags,
				    GError ** error);

G_END_DECLS
#endif /* ST_DBG_SESSION_H_INCLUDED */

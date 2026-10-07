/*
 * starter-debugger - session.c
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

#include "session.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  gchar *name;
  gchar *address;
  guint port;
  guint state;
} DebugPeer;

typedef struct
{
  gchar *id;
  gchar *description;
  gint unit;
  gdouble min;
  gdouble max;
  gdouble step;
  gdouble value;
  guint64 flags;
} DebugIo;

typedef struct
{
  gchar *name;
  gchar *describe;
} DebugVar;

struct _STDbgSession
{
  GDBusConnection *conn;
  gchar *host;
  guint16 port;

  GHashTable *peers;		/* name -> DebugPeer */
  GHashTable *inputs;		/* id -> DebugIo */
  GHashTable *outputs;		/* id -> DebugIo */
  GHashTable *vars;		/* name -> DebugVar */

  guint monitor_mask;
  guint signal_peer;
  guint signal_input;
  guint signal_output;
  guint signal_stats;
  guint signal_var;
  guint signal_stopped;

  STDbgAsyncPrintFunc async_begin;
  STDbgAsyncPrintFunc async_end;
  gpointer async_data;
};

static void
debug_peer_free (gpointer data)
{
  DebugPeer *p = data;

  if (p == NULL)
    return;
  g_free (p->name);
  g_free (p->address);
  g_free (p);
}

static void
debug_io_free (gpointer data)
{
  DebugIo *io = data;

  if (io == NULL)
    return;
  g_free (io->id);
  g_free (io->description);
  g_free (io);
}

static void
debug_var_free (gpointer data)
{
  DebugVar *v = data;

  if (v == NULL)
    return;
  g_free (v->name);
  g_free (v->describe);
  g_free (v);
}

static const gchar *
client_state_name (guint state)
{
  switch (state)
    {
    case 0:
      return "IDLE";
    case 1:
      return "PENDING";
    case 2:
      return "READY";
    case 3:
      return "FAILED";
    default:
      return "?";
    }
}

static void
monitor_printf (STDbgSession * self, guint channel, const gchar * fmt, ...)
  G_GNUC_PRINTF (3, 4);

static void
monitor_printf (STDbgSession * self, guint channel, const gchar * fmt, ...)
{
  va_list ap;

  if (self == NULL || (self->monitor_mask & channel) == 0)
    return;

  if (self->async_begin)
    self->async_begin (self->async_data);

  va_start (ap, fmt);
  {
    g_autofree gchar *msg = g_strdup_vprintf (fmt, ap);
    g_autoptr (GDateTime) now = g_date_time_new_now_local ();
    g_autofree gchar *ts = NULL;
    gint ms;

    if (now != NULL)
      {
	ts = g_date_time_format (now, "%Y-%m-%d %H:%M:%S");
	ms = g_date_time_get_microsecond (now) / 1000;
      }
    else
      {
	ts = g_strdup ("0000-00-00 00:00:00");
	ms = 0;
      }

    g_print ("[monitor %s.%03d] %s\n", ts, ms, msg);
  }
  va_end (ap);

  if (self->async_end)
    self->async_end (self->async_data);
}

static void
peers_upsert (STDbgSession * self, const gchar * name, const gchar * address,
	      guint port, guint state)
{
  DebugPeer *peer;

  peer = g_hash_table_lookup (self->peers, name);
  if (peer == NULL)
    {
      peer = g_new0 (DebugPeer, 1);
      peer->name = g_strdup (name);
      g_hash_table_insert (self->peers, peer->name, peer);
    }

  g_free (peer->address);
  peer->address = g_strdup (address ? address : "");
  peer->port = port;
  peer->state = state;
}

static void
io_upsert (GHashTable * table, const gchar * id, const gchar * description,
	   gint unit, gdouble min, gdouble max, gdouble step, gdouble value,
	   guint64 flags)
{
  DebugIo *io;

  io = g_hash_table_lookup (table, id);
  if (io == NULL)
    {
      io = g_new0 (DebugIo, 1);
      io->id = g_strdup (id);
      g_hash_table_insert (table, io->id, io);
    }

  g_free (io->description);
  io->description = g_strdup (description ? description : "");
  io->unit = unit;
  io->min = min;
  io->max = max;
  io->step = step;
  io->value = value;
  io->flags = flags;
}

static void
var_upsert (STDbgSession * self, const gchar * name, const gchar * describe)
{
  DebugVar *var;

  var = g_hash_table_lookup (self->vars, name);
  if (var == NULL)
    {
      var = g_new0 (DebugVar, 1);
      var->name = g_strdup (name);
      g_hash_table_insert (self->vars, var->name, var);
    }

  g_free (var->describe);
  var->describe = g_strdup (describe ? describe : "");
}

static void
on_peer_changed (GDBusConnection * connection, const gchar * sender_name,
		 const gchar * object_path, const gchar * interface_name,
		 const gchar * signal_name, GVariant * parameters,
		 gpointer user_data)
{
  STDbgSession *self = user_data;
  const gchar *name = NULL;
  const gchar *address = NULL;
  const gchar *event = NULL;
  guint port = 0;
  guint state = 0;

  (void) connection;
  (void) sender_name;
  (void) object_path;
  (void) interface_name;
  (void) signal_name;

  g_variant_get (parameters, "(&s&suu&s)", &name, &address, &port, &state,
		 &event);

  if (g_strcmp0 (event, "removed") == 0 || g_strcmp0 (event, "failed") == 0)
    g_hash_table_remove (self->peers, name);
  else
    peers_upsert (self, name, address, port, state);

  monitor_printf (self, ST_DBG_MON_PEERS,
		  "PeerChanged %s %s:%u %s event=%s", name,
		  address ? address : "", port, client_state_name (state),
		  event ? event : "");
}

static void
on_io_changed (STDbgSession * self, GHashTable * table, guint channel,
	       const gchar * kind, GVariant * parameters)
{
  g_autoptr (GVariant) entry = NULL;
  const gchar *id = NULL;
  const gchar *descr = NULL;
  gint unit = 0;
  gdouble min = 0, max = 0, step = 0, value = 0;
  guint64 flags = 0;

  entry = g_variant_get_child_value (parameters, 0);
  g_variant_get (entry, "{&s(&siddddt)}", &id, &descr, &unit, &min, &max,
		 &step, &value, &flags);
  io_upsert (table, id, descr, unit, min, max, step, value, flags);
  monitor_printf (self, channel, "%s %s = %g", kind, id, value);
}

static void
on_input_changed (GDBusConnection * connection, const gchar * sender_name,
		  const gchar * object_path, const gchar * interface_name,
		  const gchar * signal_name, GVariant * parameters,
		  gpointer user_data)
{
  STDbgSession *self = user_data;

  (void) connection;
  (void) sender_name;
  (void) object_path;
  (void) interface_name;
  (void) signal_name;
  on_io_changed (self, self->inputs, ST_DBG_MON_INPUTS, "InputChanged",
		 parameters);
}

static void
on_output_changed (GDBusConnection * connection, const gchar * sender_name,
		   const gchar * object_path, const gchar * interface_name,
		   const gchar * signal_name, GVariant * parameters,
		   gpointer user_data)
{
  STDbgSession *self = user_data;

  (void) connection;
  (void) sender_name;
  (void) object_path;
  (void) interface_name;
  (void) signal_name;
  on_io_changed (self, self->outputs, ST_DBG_MON_OUTPUTS, "OutputChanged",
		 parameters);
}

static void
on_stats_changed (GDBusConnection * connection, const gchar * sender_name,
		  const gchar * object_path, const gchar * interface_name,
		  const gchar * signal_name, GVariant * parameters,
		  gpointer user_data)
{
  STDbgSession *self = user_data;
  const gchar *name = NULL;

  (void) connection;
  (void) sender_name;
  (void) object_path;
  (void) interface_name;
  (void) signal_name;
  g_variant_get (parameters, "(&s)", &name);
  monitor_printf (self, ST_DBG_MON_STATS, "StatsChanged %s", name);
}

static void
on_var_changed (GDBusConnection * connection, const gchar * sender_name,
		const gchar * object_path, const gchar * interface_name,
		const gchar * signal_name, GVariant * parameters,
		gpointer user_data)
{
  STDbgSession *self = user_data;
  const gchar *name = NULL;
  const gchar *describe = NULL;

  (void) connection;
  (void) sender_name;
  (void) object_path;
  (void) interface_name;
  (void) signal_name;
  g_variant_get (parameters, "(&s&s)", &name, &describe);
  var_upsert (self, name, describe);
  monitor_printf (self, ST_DBG_MON_VARS, "VarChanged %s = %s", name, describe);
}

static void
on_stopped (GDBusConnection * connection, const gchar * sender_name,
	    const gchar * object_path, const gchar * interface_name,
	    const gchar * signal_name, GVariant * parameters,
	    gpointer user_data)
{
  STDbgSession *self = user_data;
  const gchar *reason = NULL;
  const gchar *id = NULL;
  gdouble value = 0;
  guint handle = 0;

  (void) connection;
  (void) sender_name;
  (void) object_path;
  (void) interface_name;
  (void) signal_name;
  g_variant_get (parameters, "(&s&sdu)", &reason, &id, &value, &handle);
  monitor_printf (self, ST_DBG_MON_STOPPED,
		  "Stopped reason=%s id=%s value=%g handle=%u", reason, id,
		  value, handle);
}

static void
unsubscribe_all (STDbgSession * self)
{
  if (self->conn == NULL)
    return;

  if (self->signal_peer)
    g_dbus_connection_signal_unsubscribe (self->conn, self->signal_peer);
  if (self->signal_input)
    g_dbus_connection_signal_unsubscribe (self->conn, self->signal_input);
  if (self->signal_output)
    g_dbus_connection_signal_unsubscribe (self->conn, self->signal_output);
  if (self->signal_stats)
    g_dbus_connection_signal_unsubscribe (self->conn, self->signal_stats);
  if (self->signal_var)
    g_dbus_connection_signal_unsubscribe (self->conn, self->signal_var);
  if (self->signal_stopped)
    g_dbus_connection_signal_unsubscribe (self->conn, self->signal_stopped);

  self->signal_peer = 0;
  self->signal_input = 0;
  self->signal_output = 0;
  self->signal_stats = 0;
  self->signal_var = 0;
  self->signal_stopped = 0;
}

static void
subscribe_all (STDbgSession * self)
{
  self->signal_peer =
    g_dbus_connection_signal_subscribe (self->conn, NULL, ST_DBG_INTERFACE,
					"PeerChanged", ST_DBG_OBJECT_PATH,
					NULL, G_DBUS_SIGNAL_FLAGS_NONE,
					on_peer_changed, self, NULL);
  self->signal_input =
    g_dbus_connection_signal_subscribe (self->conn, NULL, ST_DBG_INTERFACE,
					"InputChanged", ST_DBG_OBJECT_PATH,
					NULL, G_DBUS_SIGNAL_FLAGS_NONE,
					on_input_changed, self, NULL);
  self->signal_output =
    g_dbus_connection_signal_subscribe (self->conn, NULL, ST_DBG_INTERFACE,
					"OutputChanged", ST_DBG_OBJECT_PATH,
					NULL, G_DBUS_SIGNAL_FLAGS_NONE,
					on_output_changed, self, NULL);
  self->signal_stats =
    g_dbus_connection_signal_subscribe (self->conn, NULL, ST_DBG_INTERFACE,
					"StatsChanged", ST_DBG_OBJECT_PATH,
					NULL, G_DBUS_SIGNAL_FLAGS_NONE,
					on_stats_changed, self, NULL);
  self->signal_var =
    g_dbus_connection_signal_subscribe (self->conn, NULL, ST_DBG_INTERFACE,
					"VarChanged", ST_DBG_OBJECT_PATH,
					NULL, G_DBUS_SIGNAL_FLAGS_NONE,
					on_var_changed, self, NULL);
  self->signal_stopped =
    g_dbus_connection_signal_subscribe (self->conn, NULL, ST_DBG_INTERFACE,
					"Stopped", ST_DBG_OBJECT_PATH, NULL,
					G_DBUS_SIGNAL_FLAGS_NONE, on_stopped,
					self, NULL);
}

static gboolean
refresh_peers (STDbgSession * self, GError ** error)
{
  g_autoptr (GVariant) result = NULL;
  g_autoptr (GVariant) peers = NULL;
  GVariantIter iter;
  const gchar *name;
  const gchar *address;
  guint port;
  guint state;

  result =
    g_dbus_connection_call_sync (self->conn, NULL, ST_DBG_OBJECT_PATH,
				 ST_DBG_INTERFACE, "ListPeers", NULL,
				 G_VARIANT_TYPE ("(a(ssuu))"),
				 G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
  if (result == NULL)
    return FALSE;

  g_hash_table_remove_all (self->peers);
  peers = g_variant_get_child_value (result, 0);
  g_variant_iter_init (&iter, peers);
  while (g_variant_iter_next (&iter, "(&s&suu)", &name, &address, &port,
			      &state))
    peers_upsert (self, name, address, port, state);

  return TRUE;
}

static gboolean
refresh_io_table (STDbgSession * self, GHashTable * table,
		  const gchar * method, GError ** error)
{
  g_autoptr (GVariant) result = NULL;
  g_autoptr (GVariant) dict = NULL;
  GVariantIter iter;
  const gchar *id;
  const gchar *descr;
  gint unit;
  gdouble min, max, step, value;
  guint64 flags;

  result =
    g_dbus_connection_call_sync (self->conn, NULL, ST_DBG_OBJECT_PATH,
				 ST_DBG_INTERFACE, method, NULL,
				 G_VARIANT_TYPE ("(a{s(siddddt)})"),
				 G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
  if (result == NULL)
    return FALSE;

  g_hash_table_remove_all (table);
  dict = g_variant_get_child_value (result, 0);
  g_variant_iter_init (&iter, dict);
  while (g_variant_iter_next
	 (&iter, "{&s(&siddddt)}", &id, &descr, &unit, &min, &max, &step,
	  &value, &flags))
    io_upsert (table, id, descr, unit, min, max, step, value, flags);

  return TRUE;
}

static gboolean
refresh_io (STDbgSession * self, GError ** error)
{
  if (!refresh_io_table (self, self->inputs, "ListInputs", error))
    return FALSE;
  if (!refresh_io_table (self, self->outputs, "ListOutputs", error))
    return FALSE;
  return TRUE;
}

static gint
cmp_str_ptr (gconstpointer a, gconstpointer b)
{
  return g_strcmp0 (*(const gchar * const *) a, *(const gchar * const *) b);
}

static GPtrArray *
sorted_keys (GHashTable * table)
{
  GPtrArray *keys = g_ptr_array_new ();
  GHashTableIter iter;
  gpointer key;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    g_ptr_array_add (keys, key);

  g_ptr_array_sort (keys, cmp_str_ptr);
  return keys;
}

STDbgSession *
st_dbg_session_new (void)
{
  STDbgSession *self = g_new0 (STDbgSession, 1);

  self->peers =
    g_hash_table_new_full (g_str_hash, g_str_equal, NULL, debug_peer_free);
  self->inputs =
    g_hash_table_new_full (g_str_hash, g_str_equal, NULL, debug_io_free);
  self->outputs =
    g_hash_table_new_full (g_str_hash, g_str_equal, NULL, debug_io_free);
  self->vars =
    g_hash_table_new_full (g_str_hash, g_str_equal, NULL, debug_var_free);
  return self;
}

void
st_dbg_session_free (STDbgSession * self)
{
  if (self == NULL)
    return;

  st_dbg_session_disconnect (self);
  g_clear_pointer (&self->peers, g_hash_table_unref);
  g_clear_pointer (&self->inputs, g_hash_table_unref);
  g_clear_pointer (&self->outputs, g_hash_table_unref);
  g_clear_pointer (&self->vars, g_hash_table_unref);
  g_free (self->host);
  g_free (self);
}

void
st_dbg_session_set_async_print_hooks (STDbgSession * self,
				      STDbgAsyncPrintFunc begin,
				      STDbgAsyncPrintFunc end,
				      gpointer user_data)
{
  g_return_if_fail (self != NULL);
  self->async_begin = begin;
  self->async_end = end;
  self->async_data = user_data;
}

gboolean
st_dbg_session_parse_bind (const gchar * spec, gchar ** host, guint16 * port,
			   GError ** error)
{
  const gchar *colon;
  gchar *h;
  unsigned long p;
  char *end = NULL;

  g_return_val_if_fail (host != NULL, FALSE);
  g_return_val_if_fail (port != NULL, FALSE);

  if (spec == NULL || *spec == '\0')
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   "empty core address");
      return FALSE;
    }

  colon = strrchr (spec, ':');
  if (colon == NULL || colon == spec || *(colon + 1) == '\0')
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   "expected HOST:PORT, got '%s'", spec);
      return FALSE;
    }

  p = strtoul (colon + 1, &end, 10);
  if (end == colon + 1 || *end != '\0' || p == 0 || p > 65535)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   "invalid port in '%s'", spec);
      return FALSE;
    }

  h = g_strndup (spec, (gsize) (colon - spec));
  if (*h == '\0')
    {
      g_free (h);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   "empty host in '%s'", spec);
      return FALSE;
    }

  *host = h;
  *port = (guint16) p;
  return TRUE;
}

gboolean
st_dbg_session_connect (STDbgSession * self, const gchar * host, guint16 port,
			GError ** error)
{
  g_autofree gchar *addr = NULL;
  GDBusConnection *conn;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (host != NULL && *host != '\0', FALSE);
  g_return_val_if_fail (port != 0, FALSE);

  st_dbg_session_disconnect (self);

  addr = g_strdup_printf ("tcp:host=%s,port=%hu", host, port);
  conn =
    g_dbus_connection_new_for_address_sync (addr,
					    G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
					    NULL, NULL, error);
  if (conn == NULL)
    return FALSE;

  self->conn = conn;
  g_free (self->host);
  self->host = g_strdup (host);
  self->port = port;

  subscribe_all (self);
  if (!refresh_peers (self, error))
    {
      /* Keep connection; peer table may fill via signals. */
      g_clear_error (error);
    }
  if (!refresh_io (self, error))
    {
      /* Older cores without ListInputs/ListOutputs: rely on *Changed. */
      g_clear_error (error);
    }

  return TRUE;
}

void
st_dbg_session_disconnect (STDbgSession * self)
{
  if (self == NULL || self->conn == NULL)
    return;

  unsubscribe_all (self);
  g_dbus_connection_close_sync (self->conn, NULL, NULL);
  g_clear_object (&self->conn);
}

gboolean
st_dbg_session_is_connected (STDbgSession * self)
{
  return self != NULL && self->conn != NULL
    && !g_dbus_connection_is_closed (self->conn);
}

const gchar *
st_dbg_session_get_host (STDbgSession * self)
{
  g_return_val_if_fail (self != NULL, NULL);
  return self->host;
}

guint16
st_dbg_session_get_port (STDbgSession * self)
{
  g_return_val_if_fail (self != NULL, 0);
  return self->port;
}

GDBusConnection *
st_dbg_session_get_connection (STDbgSession * self)
{
  g_return_val_if_fail (self != NULL, NULL);
  return self->conn;
}

static void
print_variant_value (const gchar * key, GVariant * value)
{
  const GVariantType *type = g_variant_get_type (value);

  if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    g_print ("  %s: %s\n", key, g_variant_get_string (value, NULL));
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    g_print ("  %s: %s\n", key,
	     g_variant_get_boolean (value) ? "true" : "false");
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
    g_print ("  %s: %u\n", key, (unsigned) g_variant_get_uint16 (value));
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
    g_print ("  %s: %u\n", key, g_variant_get_uint32 (value));
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
    g_print ("  %s: %d\n", key, g_variant_get_int32 (value));
  else
    {
      g_autofree gchar *printed = g_variant_print (value, FALSE);
      g_print ("  %s: %s\n", key, printed);
    }
}

gboolean
st_dbg_session_print_status (STDbgSession * self, GError ** error)
{
  g_autoptr (GVariant) result = NULL;
  g_autoptr (GVariant) status = NULL;
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  g_return_val_if_fail (self != NULL, FALSE);

  if (!st_dbg_session_is_connected (self))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED,
		   "not connected to core debug server");
      return FALSE;
    }

  result =
    g_dbus_connection_call_sync (self->conn, NULL, ST_DBG_OBJECT_PATH,
				 ST_DBG_INTERFACE, "GetStatus", NULL,
				 G_VARIANT_TYPE ("(a{sv})"),
				 G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
  if (result == NULL)
    return FALSE;

  status = g_variant_get_child_value (result, 0);
  g_print ("Connected to %s:%u\n", self->host, (unsigned) self->port);
  g_variant_iter_init (&iter, status);
  while (g_variant_iter_next (&iter, "{&sv}", &key, &value))
    {
      print_variant_value (key, value);
      g_variant_unref (value);
    }

  return TRUE;
}

gboolean
st_dbg_session_print_peers (STDbgSession * self, GError ** error)
{
  g_autoptr (GPtrArray) keys = NULL;
  guint i;

  g_return_val_if_fail (self != NULL, FALSE);

  if (!st_dbg_session_is_connected (self))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED,
		   "not connected");
      return FALSE;
    }

  if (!refresh_peers (self, error))
    return FALSE;

  keys = sorted_keys (self->peers);
  g_print ("Peers: %u\n", keys->len);
  for (i = 0; i < keys->len; i++)
    {
      const gchar *name = keys->pdata[i];
      DebugPeer *peer = g_hash_table_lookup (self->peers, name);

      g_print ("  %-32s %s:%u  %s\n", peer->name,
	       peer->address && *peer->address ? peer->address : "-",
	       peer->port, client_state_name (peer->state));
    }

  return TRUE;
}

static void
print_io_table (GHashTable * table, const gchar * title, const gchar * filter)
{
  g_autoptr (GPtrArray) keys = NULL;
  guint i;
  guint shown = 0;

  keys = sorted_keys (table);
  for (i = 0; i < keys->len; i++)
    {
      const gchar *id = keys->pdata[i];
      DebugIo *io;

      if (filter != NULL && *filter != '\0' && strstr (id, filter) == NULL)
	continue;

      io = g_hash_table_lookup (table, id);
      if (shown == 0)
	g_print ("%s:\n", title);
      g_print ("  %-32s %g", io->id, io->value);
      if (io->description && *io->description)
	g_print ("  (%s)", io->description);
      g_print ("\n");
      shown++;
    }

  if (shown == 0 && (filter == NULL || *filter == '\0'))
    g_print ("%s: (none)\n", title);
  else if (shown == 0)
    g_print ("%s: (no match for '%s')\n", title, filter);
}

gboolean
st_dbg_session_print_io (STDbgSession * self, const gchar * filter,
			 GError ** error)
{
  g_return_val_if_fail (self != NULL, FALSE);

  if (!st_dbg_session_is_connected (self))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED,
		   "not connected");
      return FALSE;
    }

  if (!refresh_io (self, error))
    {
      /* Older cores without List*: keep signal-shadowed entries. */
      g_clear_error (error);
    }

  print_io_table (self->inputs, "Inputs", filter);
  print_io_table (self->outputs, "Outputs", filter);
  return TRUE;
}

void
st_dbg_session_print_vars (STDbgSession * self, const gchar * filter)
{
  g_autoptr (GPtrArray) keys = NULL;
  guint i;
  guint shown = 0;

  g_return_if_fail (self != NULL);

  keys = sorted_keys (self->vars);
  for (i = 0; i < keys->len; i++)
    {
      const gchar *name = keys->pdata[i];
      DebugVar *var;

      if (filter != NULL && *filter != '\0' && strstr (name, filter) == NULL)
	continue;

      var = g_hash_table_lookup (self->vars, name);
      if (shown == 0)
	g_print ("Variables:\n");
      g_print ("  %-32s %s\n", var->name,
	       var->describe ? var->describe : "");
      shown++;
    }

  if (shown == 0 && (filter == NULL || *filter == '\0'))
    g_print ("Variables: (none observed yet)\n");
  else if (shown == 0)
    g_print ("Variables: (no match for '%s')\n", filter);
}

gboolean
st_dbg_session_parse_monitor_channel (const gchar * name, guint * bits_out)
{
  guint bits = ST_DBG_MON_NONE;

  g_return_val_if_fail (bits_out != NULL, FALSE);
  if (name == NULL || *name == '\0')
    return FALSE;

  if (g_ascii_strcasecmp (name, "all") == 0)
    bits = ST_DBG_MON_ALL;
  else if (g_ascii_strcasecmp (name, "inputs") == 0
	   || g_ascii_strcasecmp (name, "input") == 0)
    bits = ST_DBG_MON_INPUTS;
  else if (g_ascii_strcasecmp (name, "outputs") == 0
	   || g_ascii_strcasecmp (name, "output") == 0)
    bits = ST_DBG_MON_OUTPUTS;
  else if (g_ascii_strcasecmp (name, "io") == 0)
    bits = ST_DBG_MON_INPUTS | ST_DBG_MON_OUTPUTS;
  else if (g_ascii_strcasecmp (name, "stats") == 0
	   || g_ascii_strcasecmp (name, "stat") == 0)
    bits = ST_DBG_MON_STATS;
  else if (g_ascii_strcasecmp (name, "vars") == 0
	   || g_ascii_strcasecmp (name, "var") == 0)
    bits = ST_DBG_MON_VARS;
  else if (g_ascii_strcasecmp (name, "peers") == 0
	   || g_ascii_strcasecmp (name, "peer") == 0)
    bits = ST_DBG_MON_PEERS;
  else if (g_ascii_strcasecmp (name, "stopped") == 0
	   || g_ascii_strcasecmp (name, "stop") == 0)
    bits = ST_DBG_MON_STOPPED;
  else
    return FALSE;

  *bits_out = bits;
  return TRUE;
}

static void
append_channel (GString * s, guint mask, guint bit, const gchar * name)
{
  if ((mask & bit) == 0)
    return;
  if (s->len > 0)
    g_string_append_c (s, ' ');
  g_string_append (s, name);
}

void
st_dbg_session_print_monitor (STDbgSession * self)
{
  g_autoptr (GString) s = NULL;

  g_return_if_fail (self != NULL);

  if (self->monitor_mask == ST_DBG_MON_NONE)
    {
      g_print ("monitor: off\n");
      return;
    }
  if (self->monitor_mask == ST_DBG_MON_ALL)
    {
      g_print ("monitor: all\n");
      return;
    }

  s = g_string_new ("");
  append_channel (s, self->monitor_mask, ST_DBG_MON_INPUTS, "inputs");
  append_channel (s, self->monitor_mask, ST_DBG_MON_OUTPUTS, "outputs");
  append_channel (s, self->monitor_mask, ST_DBG_MON_STATS, "stats");
  append_channel (s, self->monitor_mask, ST_DBG_MON_VARS, "vars");
  append_channel (s, self->monitor_mask, ST_DBG_MON_PEERS, "peers");
  append_channel (s, self->monitor_mask, ST_DBG_MON_STOPPED, "stopped");
  g_print ("monitor: %s\n", s->str);
}

void
st_dbg_session_set_monitor_mask (STDbgSession * self, guint mask)
{
  g_return_if_fail (self != NULL);
  self->monitor_mask = mask & ST_DBG_MON_ALL;
  st_dbg_session_print_monitor (self);
}

guint
st_dbg_session_get_monitor_mask (STDbgSession * self)
{
  g_return_val_if_fail (self != NULL, ST_DBG_MON_NONE);
  return self->monitor_mask;
}

void
st_dbg_session_monitor_set_channel (STDbgSession * self, guint bits,
				    gboolean enable)
{
  g_return_if_fail (self != NULL);
  bits &= ST_DBG_MON_ALL;
  if (enable)
    self->monitor_mask |= bits;
  else
    self->monitor_mask &= ~bits;
  st_dbg_session_print_monitor (self);
}

void
st_dbg_session_set_monitor (STDbgSession * self, gboolean on)
{
  st_dbg_session_set_monitor_mask (self, on ? ST_DBG_MON_ALL : ST_DBG_MON_NONE);
}

gboolean
st_dbg_session_get_monitor (STDbgSession * self)
{
  g_return_val_if_fail (self != NULL, FALSE);
  return self->monitor_mask != ST_DBG_MON_NONE;
}

static gboolean
require_connected (STDbgSession * self, GError ** error)
{
  if (st_dbg_session_is_connected (self))
    return TRUE;
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED, "not connected");
  return FALSE;
}

static gboolean
call_add_point (STDbgSession * self, const gchar * method, const gchar * id,
		guint * handle_out, GError ** error)
{
  g_autoptr (GVariant) result = NULL;
  guint handle = 0;

  if (!require_connected (self, error))
    return FALSE;

  result =
    g_dbus_connection_call_sync (self->conn, NULL, ST_DBG_OBJECT_PATH,
				 ST_DBG_INTERFACE, method,
				 g_variant_new ("(s)", id),
				 G_VARIANT_TYPE ("(u)"),
				 G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
  if (result == NULL)
    return FALSE;

  g_variant_get (result, "(u)", &handle);
  if (handle == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		   "%s failed for '%s'", method, id);
      return FALSE;
    }

  if (handle_out)
    *handle_out = handle;
  return TRUE;
}

static gboolean
call_remove_point (STDbgSession * self, const gchar * method, guint handle,
		   GError ** error)
{
  g_autoptr (GVariant) result = NULL;
  gboolean ok = FALSE;

  if (!require_connected (self, error))
    return FALSE;

  result =
    g_dbus_connection_call_sync (self->conn, NULL, ST_DBG_OBJECT_PATH,
				 ST_DBG_INTERFACE, method,
				 g_variant_new ("(u)", handle),
				 G_VARIANT_TYPE ("(b)"),
				 G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
  if (result == NULL)
    return FALSE;

  g_variant_get (result, "(b)", &ok);
  if (!ok)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
		   "unknown handle %u", handle);
      return FALSE;
    }
  return TRUE;
}

static gboolean
print_points (STDbgSession * self, const gchar * method, const gchar * title,
	      GError ** error)
{
  g_autoptr (GVariant) result = NULL;
  g_autoptr (GVariant) list = NULL;
  GVariantIter iter;
  guint handle;
  const gchar *id;
  guint n = 0;

  if (!require_connected (self, error))
    return FALSE;

  result =
    g_dbus_connection_call_sync (self->conn, NULL, ST_DBG_OBJECT_PATH,
				 ST_DBG_INTERFACE, method, NULL,
				 G_VARIANT_TYPE ("(a(us))"),
				 G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
  if (result == NULL)
    return FALSE;

  list = g_variant_get_child_value (result, 0);
  g_print ("%s:\n", title);
  g_variant_iter_init (&iter, list);
  while (g_variant_iter_next (&iter, "(u&s)", &handle, &id))
    {
      g_print ("  %u  %s\n", handle, id);
      n++;
    }
  if (n == 0)
    g_print ("  (none)\n");
  return TRUE;
}

gboolean
st_dbg_session_add_breakpoint (STDbgSession * self, const gchar * id,
			       guint * handle_out, GError ** error)
{
  return call_add_point (self, "AddBreakpoint", id, handle_out, error);
}

gboolean
st_dbg_session_remove_breakpoint (STDbgSession * self, guint handle,
				  GError ** error)
{
  return call_remove_point (self, "RemoveBreakpoint", handle, error);
}

gboolean
st_dbg_session_print_breakpoints (STDbgSession * self, GError ** error)
{
  return print_points (self, "ListBreakpoints", "Breakpoints", error);
}

gboolean
st_dbg_session_add_watchpoint (STDbgSession * self, const gchar * id,
			       guint * handle_out, GError ** error)
{
  return call_add_point (self, "AddWatchpoint", id, handle_out, error);
}

gboolean
st_dbg_session_remove_watchpoint (STDbgSession * self, guint handle,
				  GError ** error)
{
  return call_remove_point (self, "RemoveWatchpoint", handle, error);
}

gboolean
st_dbg_session_print_watchpoints (STDbgSession * self, GError ** error)
{
  return print_points (self, "ListWatchpoints", "Watchpoints", error);
}

gboolean
st_dbg_session_continue (STDbgSession * self, GError ** error)
{
  g_autoptr (GVariant) result = NULL;

  if (!require_connected (self, error))
    return FALSE;

  result =
    g_dbus_connection_call_sync (self->conn, NULL, ST_DBG_OBJECT_PATH,
				 ST_DBG_INTERFACE, "Continue", NULL, NULL,
				 G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
  return result != NULL;
}

gboolean
st_dbg_session_get_paused (STDbgSession * self, gboolean * paused,
			   GError ** error)
{
  g_autoptr (GVariant) result = NULL;
  gboolean p = FALSE;

  if (!require_connected (self, error))
    return FALSE;

  result =
    g_dbus_connection_call_sync (self->conn, NULL, ST_DBG_OBJECT_PATH,
				 ST_DBG_INTERFACE, "GetPaused", NULL,
				 G_VARIANT_TYPE ("(b)"),
				 G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
  if (result == NULL)
    return FALSE;

  g_variant_get (result, "(b)", &p);
  if (paused)
    *paused = p;
  return TRUE;
}

static gboolean
call_set_io (STDbgSession * self, const gchar * method, const gchar * id,
	     gdouble value, guint64 flags, GError ** error)
{
  g_autoptr (GVariant) result = NULL;
  gint ok = 0;

  if (!require_connected (self, error))
    return FALSE;

  result =
    g_dbus_connection_call_sync (self->conn, NULL, ST_DBG_OBJECT_PATH,
				 ST_DBG_INTERFACE, method,
				 g_variant_new ("(sdt)", id, value, flags),
				 G_VARIANT_TYPE ("(i)"),
				 G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
  if (result == NULL)
    return FALSE;

  g_variant_get (result, "(i)", &ok);
  if (ok != 1)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		   "%s '%s' failed", method, id);
      return FALSE;
    }
  return TRUE;
}

gboolean
st_dbg_session_set_input (STDbgSession * self, const gchar * id,
			  gdouble value, guint64 flags, GError ** error)
{
  return call_set_io (self, "SetInput", id, value, flags, error);
}

gboolean
st_dbg_session_set_output (STDbgSession * self, const gchar * id,
			   gdouble value, guint64 flags, GError ** error)
{
  return call_set_io (self, "SetOutput", id, value, flags, error);
}

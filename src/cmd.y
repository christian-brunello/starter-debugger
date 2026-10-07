/*
 * starter-debugger - cmd.y
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

%{
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "builtins.h"
#include "cmd-exec.h"

extern int yylex (void);
void yyerror (const char *s);

/* Set by st_dbg_exec_line before yyparse(). */
STDbgExec *st_dbg_yy_exec = NULL;
%}

%define parse.error verbose

%union {
  char *sval;
  double dval;
  unsigned uval;
}

%token T_HELP T_QUIT T_STATUS T_SOURCE T_INFO T_MONITOR
%token T_PEERS T_IO T_VARS T_ON T_OFF T_ALL T_STATS T_STOPPED
%token T_INPUTS T_OUTPUTS
%token T_BREAK T_WATCH T_DELETE T_CONTINUE T_SET T_INPUT T_OUTPUT
%token <sval> T_STRING T_WORD
%token <dval> T_NUMBER
%type  <sval> path filter_opt ident channel
%type  <uval> handle
%type  <dval> number

%destructor { g_free ($$); } <sval>

%%

input
  : %empty
  | command
  ;

command
  : T_HELP
    { st_dbg_cmd_help (st_dbg_yy_exec); }
  | T_QUIT
    { st_dbg_cmd_quit (st_dbg_yy_exec); }
  | T_STATUS
    { st_dbg_cmd_status (st_dbg_yy_exec); }
  | T_SOURCE path
    {
      st_dbg_cmd_source (st_dbg_yy_exec, $2);
      g_free ($2);
    }
  | T_INFO T_PEERS
    { st_dbg_cmd_info_peers (st_dbg_yy_exec); }
  | T_INFO T_IO filter_opt
    {
      st_dbg_cmd_info_io (st_dbg_yy_exec, $3);
      g_free ($3);
    }
  | T_INFO T_VARS filter_opt
    {
      st_dbg_cmd_info_vars (st_dbg_yy_exec, $3);
      g_free ($3);
    }
  | T_INFO T_STATUS
    { st_dbg_cmd_status (st_dbg_yy_exec); }
  | T_INFO T_BREAK
    { st_dbg_cmd_info_breakpoints (st_dbg_yy_exec); }
  | T_INFO T_WATCH
    { st_dbg_cmd_info_watchpoints (st_dbg_yy_exec); }
  | T_MONITOR
    { st_dbg_cmd_monitor (st_dbg_yy_exec, NULL, NULL); }
  | T_MONITOR T_ON
    { st_dbg_cmd_monitor (st_dbg_yy_exec, NULL, "on"); }
  | T_MONITOR T_OFF
    { st_dbg_cmd_monitor (st_dbg_yy_exec, NULL, "off"); }
  | T_MONITOR T_ALL
    { st_dbg_cmd_monitor (st_dbg_yy_exec, NULL, "all"); }
  | T_MONITOR channel
    {
      st_dbg_cmd_monitor (st_dbg_yy_exec, $2, NULL);
      g_free ($2);
    }
  | T_MONITOR channel T_ON
    {
      st_dbg_cmd_monitor (st_dbg_yy_exec, $2, "on");
      g_free ($2);
    }
  | T_MONITOR channel T_OFF
    {
      st_dbg_cmd_monitor (st_dbg_yy_exec, $2, "off");
      g_free ($2);
    }
  | T_BREAK ident
    {
      st_dbg_cmd_break (st_dbg_yy_exec, $2);
      g_free ($2);
    }
  | T_WATCH ident
    {
      st_dbg_cmd_watch (st_dbg_yy_exec, $2);
      g_free ($2);
    }
  | T_DELETE T_BREAK handle
    { st_dbg_cmd_delete_break (st_dbg_yy_exec, $3); }
  | T_DELETE T_WATCH handle
    { st_dbg_cmd_delete_watch (st_dbg_yy_exec, $3); }
  | T_CONTINUE
    { st_dbg_cmd_continue (st_dbg_yy_exec); }
  | T_SET T_INPUT ident '=' number
    {
      st_dbg_cmd_set_input (st_dbg_yy_exec, $3, $5);
      g_free ($3);
    }
  | T_SET T_OUTPUT ident '=' number
    {
      st_dbg_cmd_set_output (st_dbg_yy_exec, $3, $5);
      g_free ($3);
    }
  ;

path
  : T_STRING { $$ = $1; }
  | T_WORD   { $$ = $1; }
  ;

ident
  : T_WORD   { $$ = $1; }
  | T_STRING { $$ = $1; }
  ;

filter_opt
  : %empty   { $$ = NULL; }
  | T_WORD   { $$ = $1; }
  | T_STRING { $$ = $1; }
  ;

channel
  : T_INPUTS  { $$ = g_strdup ("inputs"); }
  | T_INPUT   { $$ = g_strdup ("input"); }
  | T_OUTPUTS { $$ = g_strdup ("outputs"); }
  | T_OUTPUT  { $$ = g_strdup ("output"); }
  | T_IO      { $$ = g_strdup ("io"); }
  | T_STATS   { $$ = g_strdup ("stats"); }
  | T_VARS    { $$ = g_strdup ("vars"); }
  | T_PEERS   { $$ = g_strdup ("peers"); }
  | T_STOPPED { $$ = g_strdup ("stopped"); }
  | T_WORD    { $$ = $1; }
  | T_STRING  { $$ = $1; }
  ;

handle
  : T_NUMBER
    {
      if ($1 < 0 || $1 != (double) (unsigned) $1)
	{
	  yyerror ("handle must be a positive integer");
	  YYERROR;
	}
      $$ = (unsigned) $1;
    }
  ;

number
  : T_NUMBER { $$ = $1; }
  ;

%%

void
yyerror (const char *s)
{
  g_printerr ("parse error: %s\n", s);
}

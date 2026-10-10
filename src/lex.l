/*
 * starter-debugger - lex.l
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
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
%}

%option noyywrap nounput noinput

NUMBER  [-+]?[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?
WORD    [^ \t\r\n#"'=]+
QSTR    \"([^\"\\]|\\.)*\"
SQUOTE  '([^'\\]|\\.)*'

%%

[ \t\r]+                { /* skip */ }
"#".*                   { /* comment to end of line */ }

"="                     { return '='; }

(?i:help)               { return T_HELP; }
(?i:h)                  { return T_HELP; }
(?i:quit)               { return T_QUIT; }
(?i:exit)               { return T_QUIT; }
(?i:q)                  { return T_QUIT; }
(?i:status)             { return T_STATUS; }
(?i:source)             { return T_SOURCE; }
(?i:info)               { return T_INFO; }
(?i:monitor)            { return T_MONITOR; }
(?i:peers)              { return T_PEERS; }
(?i:io)                 { return T_IO; }
(?i:vars)               { return T_VARS; }
(?i:on)                 { return T_ON; }
(?i:off)                { return T_OFF; }
(?i:all)                { return T_ALL; }
(?i:stats)              { return T_STATS; }
(?i:stopped)            { return T_STOPPED; }
(?i:inputs)             { return T_INPUTS; }
(?i:outputs)            { return T_OUTPUTS; }
(?i:rules)              { return T_RULES; }
(?i:triggers)           { return T_TRIGGERS; }
(?i:engine)             { return T_ENGINE; }
(?i:break)              { return T_BREAK; }
(?i:b)                  { return T_BREAK; }
(?i:watch)              { return T_WATCH; }
(?i:w)                  { return T_WATCH; }
(?i:delete)             { return T_DELETE; }
(?i:continue)           { return T_CONTINUE; }
(?i:c)                  { return T_CONTINUE; }
(?i:set)                { return T_SET; }
(?i:input)              { return T_INPUT; }
(?i:output)             { return T_OUTPUT; }

{NUMBER}                {
                          yylval.dval = g_ascii_strtod (yytext, NULL);
                          return T_NUMBER;
                        }

{QSTR}                  {
                          size_t n = yyleng;
                          gchar *s;

                          if (n >= 2)
                            s = g_strndup (yytext + 1, n - 2);
                          else
                            s = g_strdup ("");
                          yylval.sval = s;
                          return T_STRING;
                        }

{SQUOTE}                {
                          size_t n = yyleng;
                          gchar *s;

                          if (n >= 2)
                            s = g_strndup (yytext + 1, n - 2);
                          else
                            s = g_strdup ("");
                          yylval.sval = s;
                          return T_STRING;
                        }

{WORD}                  {
                          yylval.sval = g_strdup (yytext);
                          return T_WORD;
                        }

\n                      { /* end of line buffer */ }

.                       {
                          g_printerr ("unexpected character: '%c'\n", yytext[0]);
                          return yytext[0];
                        }

%%

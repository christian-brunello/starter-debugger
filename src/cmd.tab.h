/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_ST_DBG_YY_CMD_TAB_H_INCLUDED
# define YY_ST_DBG_YY_CMD_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef ST_DBG_YYDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define ST_DBG_YYDEBUG 1
#  else
#   define ST_DBG_YYDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define ST_DBG_YYDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined ST_DBG_YYDEBUG */
#if ST_DBG_YYDEBUG
extern int st_dbg_yydebug;
#endif

/* Token kinds.  */
#ifndef ST_DBG_YYTOKENTYPE
# define ST_DBG_YYTOKENTYPE
  enum st_dbg_yytokentype
  {
    ST_DBG_YYEMPTY = -2,
    ST_DBG_YYEOF = 0,              /* "end of file"  */
    ST_DBG_YYerror = 256,          /* error  */
    ST_DBG_YYUNDEF = 257,          /* "invalid token"  */
    T_HELP = 258,                  /* T_HELP  */
    T_QUIT = 259,                  /* T_QUIT  */
    T_STATUS = 260,                /* T_STATUS  */
    T_SOURCE = 261,                /* T_SOURCE  */
    T_STRING = 262,                /* T_STRING  */
    T_WORD = 263                   /* T_WORD  */
  };
  typedef enum st_dbg_yytokentype st_dbg_yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined ST_DBG_YYSTYPE && ! defined ST_DBG_YYSTYPE_IS_DECLARED
union ST_DBG_YYSTYPE
{
#line 38 "cmd.y"

  char *sval;

#line 84 "cmd.tab.h"

};
typedef union ST_DBG_YYSTYPE ST_DBG_YYSTYPE;
# define ST_DBG_YYSTYPE_IS_TRIVIAL 1
# define ST_DBG_YYSTYPE_IS_DECLARED 1
#endif


extern ST_DBG_YYSTYPE st_dbg_yylval;


int st_dbg_yyparse (void);


#endif /* !YY_ST_DBG_YY_CMD_TAB_H_INCLUDED  */

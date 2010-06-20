%{
#include <apr_hash.h>
#include <apr_pools.h>
#include <apr_tables.h>
#include <stdarg.h>

#include "apr_macros.h"

#include "jxtl.h"
#include "json_node.h"
#include "json_writer.h"
#include "json.h"

/*
 * Define YY_DECL before including jxtl_lex.h so that it knows we are doing a
 * custom declaration of jxtl_lex.
 */
#define YY_DECL

#include "jxtl_parse.h"
#include "jxtl_lex.h"

#define text_handler callbacks->text_handler
#define section_start_handler callbacks->section_start_handler
#define section_end_handler callbacks->section_end_handler
#define separator_start_handler callbacks->separator_start_handler
#define separator_end_handler callbacks->separator_end_handler
#define if_start_handler callbacks->if_start_handler
#define else_handler callbacks->else_handler
#define if_end_handler callbacks->if_end_handler
#define test_handler callbacks->test_handler
#define value_handler callbacks->value_handler
#define user_data callbacks->user_data

void jxtl_error( YYLTYPE *yylloc, yyscan_t scanner, jxtl_callback_t *callbacks,
                 const char *error_string, ... );
%}

%name-prefix="jxtl_"
%defines
%verbose
%locations
%error-verbose
%pure-parser

%parse-param { yyscan_t scanner }
%parse-param { jxtl_callback_t *callbacks }
%lex-param { yyscan_t scanner }

%union {
  int ival;
  unsigned char *string;
}

%token T_DIRECTIVE_START "{{" T_DIRECTIVE_END "}}"
       T_SECTION "section" T_SEPARATOR "separator" T_TEST "test"
       T_END "end" T_IF "if" T_ELSE "else"
%token <string> T_TEXT "text" T_IDENTIFIER "identifier" T_STRING "string"

%type <ival> negate

%%

document
  : text
;

text
  : /* empty */
  | text T_TEXT { text_handler( user_data, $<string>2 ); }
  | text value_directive
  | text section_directive
  | text if_directive
;

value_directive
  : T_DIRECTIVE_START T_IDENTIFIER T_DIRECTIVE_END
    { value_handler( user_data, $<string>2 ); }
;

section_directive
  : T_DIRECTIVE_START T_SECTION T_IDENTIFIER
    { section_start_handler( user_data, $<string>3 ); }
    options
    T_DIRECTIVE_END
    section_content
    T_DIRECTIVE_START T_END T_DIRECTIVE_END
    { section_end_handler( user_data, $<string>3 ); }
;

if_directive
  : T_DIRECTIVE_START T_IF '(' negate T_IDENTIFIER ')' T_DIRECTIVE_END
    {
      if_start_handler( user_data, $<string>5, $<ival>4 );
    }
    section_content
    end_if
;

end_if
  : T_DIRECTIVE_START T_ELSE T_DIRECTIVE_END
    {
      else_handler( user_data );
    }
    section_content
    T_DIRECTIVE_START T_END T_DIRECTIVE_END
    {
      if_end_handler( user_data );
    }
  | T_DIRECTIVE_START T_END T_DIRECTIVE_END
    {
      if_end_handler( user_data );
    }
;

section_content
  : /* empty */
  | section_content T_TEXT { text_handler( user_data, $<string>2 ); }
  | section_content value_directive
  | section_content section_directive
  | section_content if_directive
;

options
  : /* empty */
  | option
  | options ',' option
;

option
  : T_SEPARATOR '=' T_STRING
    {
      separator_start_handler( user_data );
      text_handler( user_data, $<string>3 );
      separator_end_handler( user_data );
    }
  | T_TEST '=' '(' negate T_IDENTIFIER ')'
    {
      test_handler( user_data, $<string>5, $<ival>4 );
    }
;

negate
  : /* empty */ { $$ = 0; }
  | '!' { $$ = 1; }
;


%%

void jxtl_error( YYLTYPE *yylloc, yyscan_t scanner, jxtl_callback_t *callbacks,
                 const char *error_string, ... )
{
  va_list args;
  fprintf( stderr, "%d: ", yylloc->first_line );
  va_start( args, error_string);
  vfprintf( stderr, error_string, args );
  va_end( args );
  fprintf( stderr, " near column %d\n", yylloc->first_column + 1 );
}

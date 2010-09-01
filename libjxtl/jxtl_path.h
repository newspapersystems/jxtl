#ifndef JXTL_PATH_H
#define JXTL_PATH_H

#include <apr_pools.h>

#include "jxtl_path_parse.h"
#include "jxtl_path_lex.h"

#include "lex_extra.h"
#include "json.h"

/*
 * Parser callback prototypes.
 */
typedef struct jxtl_path_callback_t {
  void ( *identifier_handler )( void *user_data, unsigned char *ident );
  void ( *root_object_handler )( void *user_data );
  void ( *parent_object_handler )( void *user_data );
  void ( *current_object_handler )( void *user_data );
  void ( *any_object_handler )( void *user_data );
  void ( *predicate_start_handler )( void *user_data );
  void ( *predicate_end_handler )( void *user_data );
  void ( *negate_handler )( void *user_data );
  void *user_data;
} jxtl_path_callback_t;

typedef enum jxtl_path_expr_type {
  JXTL_PATH_BOOLEAN_EXPR,
  JXTL_PATH_ROOT_OBJ,
  JXTL_PATH_PARENT_OBJ,
  JXTL_PATH_CURRENT_OBJ,
  JXTL_PATH_ANY_OBJ,
  JXTL_PATH_LOOKUP,
}jxtl_path_expr_type;

typedef struct jxtl_path_expr_t {
  /** What type of expression this is. */
  jxtl_path_expr_type type;
  /** A name to lookup. */
  unsigned char *identifier;
  /** The beginning of this expression. */
  struct jxtl_path_expr_t *root;
  /** Next expression. */
  struct jxtl_path_expr_t *next;
  /** A predicate to evaluate. */
  struct jxtl_path_expr_t *predicate;
  /** Whether or not this expression should be negated. */
  int negate;
}jxtl_path_expr_t;


typedef struct jxtl_data {
  apr_pool_t *mp;
  /** Array to store the current expression.  */
  apr_array_header_t *expr_array;
  /** Root of the path expression. */
  jxtl_path_expr_t *root;
  /** The current expression. */ 
  jxtl_path_expr_t *curr;
}jxtl_data;

typedef struct jxtl_path_builder_t {
  apr_pool_t *mp;
  jxtl_path_callback_t callbacks;
  yyscan_t scanner;
  lex_extra_t lex_extra;
  jxtl_data data;
}jxtl_path_builder_t;

typedef struct jxtl_path_obj_t {
  apr_pool_t *mp;
  apr_array_header_t *nodes;
}jxtl_path_obj_t;

jxtl_path_builder_t *jxtl_path_builder_create( apr_pool_t *mp );
jxtl_path_obj_t *jxtl_path_obj_create( apr_pool_t *mp );

jxtl_path_expr_t *jxtl_path_compile( jxtl_path_builder_t *path_builder,
				     const unsigned char *path );
int jxtl_path_eval( apr_pool_t *mp, const unsigned char *path, json_t *json,
		    jxtl_path_obj_t **obj_ptr );
int jxtl_path_compiled_eval( apr_pool_t *mp, jxtl_path_expr_t *expr,
                             json_t *json, jxtl_path_obj_t **obj_ptr );

#endif

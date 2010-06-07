#include <apr_getopt.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_tables.h>

#include "apr_macros.h"

#include "json.h"
#include "json_node.h"
#include "json_writer.h"

#include "jxtl.h"
#include "jxtl_parse.h"
#include "jxtl_lex.h"
#include "lex_extra.h"
#include "xml2json.h"

/** Constants used for calling the print function */
typedef enum section_print_type {
  PRINT_SECTION,
  PRINT_SEPARATOR
} section_print_type;

typedef enum jxtl_content_type {
  JXTL_TEXT,
  JXTL_SECTION,
  JXTL_VALUE
} jxtl_content_type;

typedef struct jxtl_test_t {
  unsigned char *name;
  int negate;
} jxtl_test_t;

typedef struct jxtl_section_t {
  /** Name of the section. */
  unsigned char *name; 
  /** Array of the content in the section. */
  apr_array_header_t *content;
  /** Array of content for the separator. */
  apr_array_header_t *separator;
  /** A pointer to the content or separator array. */
  apr_array_header_t *current_array;
  /** A test to run when iterating over a section */
  jxtl_test_t *test;
} jxtl_section_t;

typedef struct jxtl_content_t {
  /** What this content contains in its value pointer. */
  jxtl_content_type type;
  /** Either a string or a pointer to a jxtl_section_t */
  void *value; 
} jxtl_content_t;

/**
 * Structure to hold data during parsing.  One of these will be passed to the
 * callback functions.
 */
typedef struct jxtl_data_t {
  /** Memory pool */
  apr_pool_t *mp;
  /** Section depth */
  int depth;
  /** Pointer to the JSON object */
  json_t *json;
  /** Array of jxtl_section_t objects. */
  apr_array_header_t *section;
  /** Reusable array of json objects.  Used when printing a section. */
  apr_array_header_t *json_array;
} jxtl_data_t;

void json_value_print( json_t *json )
{
  if ( !json )
    return;

  switch ( json->type ) {
  case JSON_STRING:
    printf( "%s", json->value.string );
    break;
    
  case JSON_INTEGER:
    printf( "%d", json->value.integer );
    break;
    
  case JSON_NUMBER:
    printf( "%g", json->value.number );
    break;

  default:
    fprintf( stderr, "error: cannot get value of object or array\n" );
    break;
  }
}

json_t *json_lookup( apr_array_header_t *json_array,
		     char *name )
{
  int i;
  json_t *json_value;
  json_t *json_value2;

  for ( i = json_array->nelts - 1; i >= 0; i-- ) {
    json_value = APR_ARRAY_IDX( json_array, i, json_t * );
    if ( json_value->type == JSON_OBJECT ) {
      json_value2 = apr_hash_get( json_value->value.object, name,
				  APR_HASH_KEY_STRING );
      if ( json_value2 ) {
	return json_value2;
      }
    }
    else if ( ( json_value->type != JSON_ARRAY ) &&
	      ( apr_strnatcasecmp( name, "." ) == 0 ) ) {
      return json_value;
    }
  }
  return NULL;
}

void text_print( char *text, jxtl_content_t *prev_content,
                 jxtl_content_t *next_content,
                 section_print_type print_type )
{
  char *text_ptr = text;
  int len = strlen( text_ptr );

  if ( ( print_type == PRINT_SECTION ) && ( !prev_content ) &&
       ( text_ptr[0] == '\n' ) ) {
    text_ptr++;
    len--;
  }
  if ( ( print_type == PRINT_SECTION ) && ( !next_content ) &&
       ( text_ptr[len - 1] == '\n' ) ) {
    len--;
  }
  printf( "%.*s", len, text_ptr );
}

/**
 * Evaluate whether or not to expand "section" for "json".
 */
int section_test( jxtl_section_t *section, json_t *json )
{
  int result;
  jxtl_test_t *test = section->test;

  /* No test, return true */
  if ( !test )
    return 1;
  
  if ( json->type == JSON_OBJECT ) {
    result = ( apr_hash_get( json->value.object, test->name,
                             APR_HASH_KEY_STRING ) != NULL );
  }

  result = ( test->negate ) ? !result : result;

  return result;
}

/**
 * Count up the number of array items that pass the test for "section".  This
 * is necessary so we know if the separator should be printed or not.
 */
int num_true_array_items( jxtl_section_t *section, json_t *json )
{
  int i;
  int num = 0;
  json_t *json_value;

  for ( i = 0; i < json->value.array->nelts; i++ ) {
    json_value = APR_ARRAY_IDX( json->value.array, i, json_t * );
    if ( section_test( section, json_value ) ) {
      num++;
    }
  }

  return num;
}

/**
 * Print a saved section
 */
void jxtl_section_print( jxtl_section_t *section,
                         apr_array_header_t *json_array,
                         section_print_type print_type )
{
  int i;
  int num_items;
  int num_printed;
  jxtl_content_t *prev_content, *content, *next_content;
  jxtl_section_t *tmp_section;
  json_t *json;
  json_t *json_value;
  json_t *json_value2;
  apr_array_header_t *content_array;

  json = APR_ARRAY_IDX( json_array, json_array->nelts - 1, json_t * );

  if ( !json )
    return;

  if ( print_type == PRINT_SECTION )
    content_array = section->content;
  else
    content_array = section->separator;

  /*
   * When we have an array, just loop over the items in it and call
   * jxtl_section_print() for each item.
   */
  if ( json->type == JSON_ARRAY ) {
    num_items = num_true_array_items( section, json );
    num_printed = 0;
    for ( i = 0; i < json->value.array->nelts; i++ ) {
      json_value = APR_ARRAY_IDX( json->value.array, i, json_t * );
      if ( section_test( section, json_value ) ) {
        APR_ARRAY_PUSH( json_array, json_t * ) = json_value;
        jxtl_section_print( section, json_array, PRINT_SECTION );

        num_printed++;
        /* Only print the separator if it's not the last one */
        if ( num_printed < num_items )
          jxtl_section_print( section, json_array, PRINT_SEPARATOR );

        APR_ARRAY_POP( json_array, json_t * );
      }
    }
    return;
  }

  prev_content = NULL;
  next_content = NULL;

  for ( i = 0; i < content_array->nelts; i++ ) {
    content = APR_ARRAY_IDX( content_array, i, jxtl_content_t * );
    next_content = ( i + 1 < content_array->nelts ) ?
                   APR_ARRAY_IDX( content_array, i + 1, jxtl_content_t * ) :
                   NULL;
    switch ( content->type ) {
    case JXTL_TEXT:
      text_print( content->value, prev_content, next_content, print_type );
      break;

    case JXTL_SECTION:
      tmp_section = (jxtl_section_t *) content->value;
      if ( json->type == JSON_OBJECT ) {
	json_value = apr_hash_get( json->value.object, tmp_section->name,
				   APR_HASH_KEY_STRING );
	if ( json_value ) {
	  APR_ARRAY_PUSH( json_array, json_t * ) = json_value;
	  jxtl_section_print( tmp_section, json_array, PRINT_SECTION );
	  APR_ARRAY_POP( json_array, json_t * );
	}
      }
      break;

    case JXTL_VALUE:
      json_value_print( json_lookup( json_array, (char *) content->value ) );
      break;
    }
    prev_content = content;
  }
}

/*
 * Convenience function to create a new content object and it on the current
 * array.
 */
void jxtl_content_push( jxtl_data_t *data, jxtl_content_type type,
			  void *value )
{
  jxtl_content_t *content = NULL;
  jxtl_section_t *section = NULL;

  content = apr_palloc( data->mp, sizeof( jxtl_content_t ) );
  content->type = type;
  content->value = value;

  /*
   * Push the new content object into the current section.  Then check if the
   * content is actually a section, if it is then we want to add it to the
   * current section array.
   */

  section = APR_ARRAY_IDX( data->section, data->section->nelts - 1,
			   jxtl_section_t * );
  if ( section ) {
    APR_ARRAY_PUSH( section->current_array, jxtl_content_t * ) = content;
  }

  if ( content->type == JXTL_SECTION ) {
    APR_ARRAY_PUSH( data->section, jxtl_section_t * ) = value;
  }
}

/**
 * Parser callback for when it finds text.
 * @param user_data The jxtl_data.
 * @param text The text.
 */
void jxtl_text_func( void *user_data, unsigned char *text )
{
  jxtl_data_t *data = (jxtl_data_t *) user_data;

  if ( data->depth > 0 ) {
    /* Save off the text if we are nested. */
    jxtl_content_push( data, JXTL_TEXT,
			 apr_pstrdup( data->mp, (char *) text ) );
  }
  else {
    /* Not inside any sections, so just print the text. */
    printf( "%s", text );
  }
}

void jxtl_section_start( void *user_data, unsigned char *name )
{
  jxtl_data_t *data = (jxtl_data_t *) user_data;
  jxtl_section_t *section;

  section = apr_palloc( data->mp, sizeof( jxtl_section_t ) );
  section->name = (unsigned char *) apr_pstrdup( data->mp, (char *) name );
  section->content = apr_array_make( data->mp, 1024,
				     sizeof( jxtl_content_t * ) );
  section->separator = apr_array_make( data->mp, 1024,
				       sizeof( jxtl_content_t * ) );
  section->current_array = section->content;
  section->test = NULL;
  jxtl_content_push( data, JXTL_SECTION, section );
  data->depth++;
}

/**
 * Parser callback for when a section ends.
 * @param user_data The jxtl_data.
 * @param name The name of the section.
 */
void jxtl_section_end( void *user_data, unsigned char *name )
{
  jxtl_data_t *data = (jxtl_data_t *) user_data;
  jxtl_section_t *section;
  json_t *json;

  section = APR_ARRAY_POP( data->section, jxtl_section_t * );
  data->depth--;

  if ( data->depth == 0 ) {
    /* Process saved document fragment */
    json = apr_hash_get( data->json->value.object, section->name,
			 APR_HASH_KEY_STRING );
    APR_ARRAY_CLEAR( data->json_array );
    APR_ARRAY_PUSH( data->json_array, json_t * ) = json;

    if ( json )
      jxtl_section_print( section, data->json_array, PRINT_SECTION );

    APR_ARRAY_POP( data->json_array, json_t * );
    /* Clear the pool when we finish a section. */
    APR_ARRAY_CLEAR( data->section );
    apr_pool_clear( data->mp );
  }
}

/**
 * Parser callback for when it encounters a separator directive.  All this does
 * is take the current section and set its current_array to the separator.
 * @param user_data The jxtl_data.
 */
void jxtl_separator_start( void *user_data )
{
  jxtl_data_t *data = (jxtl_data_t *) user_data;
  jxtl_section_t *section;
  section = APR_ARRAY_IDX( data->section, data->section->nelts - 1,
			   jxtl_section_t * );
  section->current_array = section->separator;
}

/**
 * Parser callback for when a separator directive is ended.  Just sets the
 * current_array of the section back to the content.
 * @param user_data The jxtl_data.
 */
void jxtl_separator_end( void *user_data )
{
  jxtl_data_t *data = (jxtl_data_t *) user_data;
  jxtl_section_t *section;
  section = APR_ARRAY_IDX( data->section, data->section->nelts - 1,
			   jxtl_section_t * );
  section->current_array = section->content;
}

/**
 * Parser callback function for when it encounters a value reference in the
 * template, i.e. {{value}}.  If we are not nested at all, it is printed
 * immediately.  Otherwise, the name is just saved off for later processing.
 * @param user_data The jxtl_data.
 * @param name The name of the value to lookup.
 */
void jxtl_value_func( void *user_data, unsigned char *name )
{
  jxtl_data_t *data = (jxtl_data_t *) user_data;
  json_t *json_value;

  if ( data->depth > 0 ) {
    jxtl_content_push( data, JXTL_VALUE,
                       apr_pstrdup( data->mp, (char *) name ) );
  }
  else {
    json_value = apr_hash_get( data->json->value.object, name,
			       APR_HASH_KEY_STRING );
    json_value_print( json_value );
  }
}

/**
 * Parser callback function for when it encounters a test expression.
 * @param user_data The jxtl_data
 * @param name The name to lookup when doing the test
 * @param negate Whether or not to negate the result of the lookup.
 */
void jxtl_test_func( void *user_data, unsigned char *name, int negate )
{
  jxtl_data_t *data = (jxtl_data_t *) user_data;
  jxtl_test_t *test;
  jxtl_section_t *section;

  section = APR_ARRAY_IDX( data->section, data->section->nelts - 1,
			   jxtl_section_t * );

  test = apr_palloc( data->mp, sizeof( jxtl_test_t ) );
  test->name = apr_pstrdup( data->mp, (char *) name );
  test->negate = negate;
  section->test = test;
}

void jxtl_usage( const char *prog_name,
                 const apr_getopt_option_t *options )
{
  int i;

  printf( "Usage: %s [options]\n", prog_name );
  printf( "  Options:\n" );
  for ( i = 0; options[i].name; i++ ) {
    printf( "    -%c, --%s %s\n", options[i].optch, options[i].name,
	    options[i].description );
  }
}

/**
 * Read in the command line arguments to set the template file and the
 * data_file.
 */
void jxtl_init( int argc, char const * const *argv , apr_pool_t *mp,
		  const char **template_file, const char **json_file,
		  const char **xml_file, int *skip_root )
{
  int i;
  apr_getopt_t *options;
  apr_status_t ret;
  int ch;
  const char *arg;
  const apr_getopt_option_t jxtl_options[] = {
    { "template", 't', 1, "template file" },
    { "json", 'j', 1, "JSON data dictionary for template" },
    { "xml", 'x', 1, "XML data dictionary for template" },
    { "skiproot", 's', 0,
      "Skip the root element if using an XML data dictionary" },
    { 0, 0, 0, 0 }
  };

  *template_file = NULL;
  *json_file = NULL;
  *xml_file = NULL;
  *skip_root = 0;

  apr_getopt_init( &options, mp, argc, argv );

  while ( ( ret = apr_getopt_long( options, jxtl_options, &ch,
				   &arg ) ) == APR_SUCCESS ) {
    switch ( ch ) {
    case 'j':
      *json_file = arg;
      break;

    case 'x':
      *xml_file = arg;
      break;

    case 's':
      *skip_root = 1;
      break;

    case 't':
      *template_file = arg;
      break;
    }
  }

  if ( ( ret == APR_BADCH ) || ( *template_file == NULL ) ||
       ( ( *json_file == NULL ) && ( *xml_file == NULL ) ) ) {
    jxtl_usage( argv[0], jxtl_options );
    exit( EXIT_FAILURE );
  }
}

/**
 * Load data from either json_file or xml_file.  One of those has to be
 * non-null.
 */
int jxtl_load_data( const char *json_file, const char *xml_file,
                      int skip_root, json_writer_t *writer )
{
  int ret = 1;

  if ( xml_file ) {
    ret = xml_file_read( xml_file, writer, skip_root );
  }
  else if ( json_file ) {
    ret = json_file_parse( json_file, writer );
  }

  return ret;
}

int main( int argc, char const * const *argv )
{
  apr_pool_t *mp;
  int parse_result;
  lex_extra_t lex_extra;
  yyscan_t jxtl_scanner;
  jxtl_data_t jxtl_data;
  const char *template_file = NULL;
  const char *json_file = NULL;
  const char *xml_file = NULL;
  int skip_root;
  json_writer_t writer;

  apr_app_initialize( NULL, NULL, NULL );
  apr_pool_create( &mp, NULL );

  jxtl_init( argc, argv, mp, &template_file, &json_file, &xml_file,
             &skip_root );

  /*
   * Setup our callback object.  Note that the stack is created using this
   * memory pool, and not its internal one.  The reason is then when we are
   * done processing sections, we want to be able to clear all of that memory
   * quickly and leave the section array allocated.
   */
  apr_pool_create( &jxtl_data.mp, NULL );
  jxtl_data.depth = 0;
  jxtl_data.json = NULL;
  jxtl_data.section = apr_array_make( mp, 1024,
					sizeof( jxtl_section_t * ) );
  jxtl_data.json_array = apr_array_make( mp, 1024, sizeof( json_t * ) );

  jxtl_callback_t callbacks = {
    jxtl_text_func,
    jxtl_section_start,
    jxtl_section_end,
    jxtl_separator_start,
    jxtl_separator_end,
    jxtl_test_func,
    jxtl_value_func,
    &jxtl_data
  };

  json_writer_init( &writer );

  if ( jxtl_load_data( json_file, xml_file, skip_root, &writer ) == 0 ) {
    jxtl_data.json = writer.json;
    jxtl_lex_init( &jxtl_scanner );
    lex_extra_init( &lex_extra, template_file );
    jxtl_set_extra( &lex_extra, jxtl_scanner );
    parse_result = jxtl_parse( jxtl_scanner, &callbacks );
    lex_extra_destroy( &lex_extra );
    jxtl_lex_destroy( jxtl_scanner );
  }

  json_writer_destroy( &writer );

  apr_pool_destroy( jxtl_data.mp );
  apr_pool_destroy( mp );
  apr_terminate();
  return parse_result;
}
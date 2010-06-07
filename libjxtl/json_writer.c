#include <stdio.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_tables.h>

#include "apr_macros.h"
#include "json_node.h"
#include "json_writer.h"

void json_writer_ctx_init( json_writer_ctx_t *context )
{
  apr_pool_create( &context->mp, NULL );
  context->depth = 0;
  context->prop_stack = apr_array_make( context->mp, 1024,
					sizeof( unsigned char * ) );
  context->state_stack = apr_array_make( context->mp, 1024,
					 sizeof( json_state ) );
  APR_ARRAY_PUSH( context->state_stack, json_state ) = JSON_INITIAL;
}

void json_writer_ctx_destroy( json_writer_ctx_t *context )
{
  apr_pool_destroy( context->mp );
  context->mp = NULL;
  context->depth = 0;
  context->prop_stack = NULL;
  context->state_stack = NULL;
}

json_state json_writer_ctx_state_get( json_writer_ctx_t *context )
{
  return APR_ARRAY_IDX( context->state_stack, context->state_stack->nelts - 1,
			json_state );
}

int json_writer_ctx_can_start_object_or_array( json_writer_ctx_t *context )
{
  json_state state = json_writer_ctx_state_get( context );
  return ( state == JSON_INITIAL || state == JSON_PROPERTY ||
	   state == JSON_IN_ARRAY );
}

unsigned char *json_writer_ctx_prop_get( json_writer_ctx_t *context )
{
  return APR_ARRAY_IDX( context->prop_stack, context->prop_stack->nelts - 1,
			unsigned char * );
}

int json_writer_ctx_object_start( json_writer_ctx_t *context )
{
  if ( !json_writer_ctx_can_start_object_or_array( context ) )
    return 0;

  APR_ARRAY_PUSH( context->state_stack, json_state ) = JSON_IN_OBJECT;
  return 1;
}

int json_writer_ctx_object_end( json_writer_ctx_t *context )
{
  if ( json_writer_ctx_state_get( context ) != JSON_IN_OBJECT )
    return 0;

  apr_array_pop( context->state_stack );
  return 1;
}

int json_writer_ctx_array_start( json_writer_ctx_t *context )
{
  if ( !json_writer_ctx_can_start_object_or_array( context ) )
    return 0;

  APR_ARRAY_PUSH( context->state_stack, json_state ) = JSON_IN_ARRAY;
  return 1;
}

int json_writer_ctx_array_end( json_writer_ctx_t *context )
{
  if ( json_writer_ctx_state_get( context ) != JSON_IN_ARRAY )
    return 0;

  apr_array_pop( context->state_stack );
  return 1;
}

int json_writer_ctx_property_start( json_writer_ctx_t *context,
				    unsigned char *name )
{
  unsigned char *name_copy;
  if ( json_writer_ctx_state_get( context ) != JSON_IN_OBJECT )
    return 0;

  name_copy = (unsigned char *) apr_pstrdup( context->mp, (char *) name );

  APR_ARRAY_PUSH( context->prop_stack, unsigned char * ) = name_copy;
  APR_ARRAY_PUSH( context->state_stack, json_state ) = JSON_PROPERTY;
  return 1;
}

int json_writer_ctx_property_end( json_writer_ctx_t *context )
{
  apr_array_pop( context->prop_stack );
  apr_array_pop( context->state_stack );
  return 1;
}

int json_writer_ctx_can_write_value( json_writer_ctx_t *context )
{
  json_state state = json_writer_ctx_state_get( context );
  return ( state == JSON_PROPERTY || state == JSON_IN_ARRAY );
}

void json_writer_init( json_writer_t *writer )
{
  apr_pool_create( &writer->mp, NULL );
  writer->context = apr_palloc( writer->mp, sizeof( json_writer_ctx_t ) );
  json_writer_ctx_init( writer->context );
  writer->json = NULL;
  writer->json_stack = apr_array_make( writer->mp, 1024, sizeof( json_t * ) );
}

void json_writer_destroy( json_writer_t *writer )
{
  json_writer_ctx_destroy( writer->context );
  apr_pool_destroy( writer->mp );
  writer->mp = NULL;
  writer->json = NULL;
}

static void json_writer_error( const char *error_string, ... )
{
  va_list args;
  fprintf( stderr, "json_writer error:  " );
  va_start( args, error_string);
  vfprintf( stderr, error_string, args );
  fprintf( stderr, "\n" );
  va_end( args );
}

static void json_add( json_writer_t *writer, json_t *json )
{
  json_t *obj = NULL;
  json_t *tmp_json;
  json_t *new_array;

  obj = APR_ARRAY_IDX( writer->json_stack, writer->json_stack->nelts - 1,
                       json_t * );

  if ( !obj ) {
    writer->json = json;
    return;
  }

  switch ( obj->type ) {
  case JSON_OBJECT:
    JSON_NAME( json ) = json_writer_ctx_prop_get( writer->context );
    tmp_json = apr_hash_get( obj->value.object, JSON_NAME( json ),
			     APR_HASH_KEY_STRING );
    if ( tmp_json && tmp_json->type != JSON_ARRAY ) {
      /* Key already exists, make an array and put both objects in it. */
      new_array = json_array_create( writer->mp );
      JSON_NAME( new_array ) = JSON_NAME( json );
      JSON_NAME( json ) = NULL;
      JSON_NAME( tmp_json ) = NULL;
      APR_ARRAY_PUSH( new_array->value.array, json_t * ) = tmp_json;
      APR_ARRAY_PUSH( new_array->value.array, json_t * ) = json;
      apr_hash_set( obj->value.object, JSON_NAME( new_array ),
		    APR_HASH_KEY_STRING, new_array );
    }
    else if ( tmp_json && tmp_json->type == JSON_ARRAY ) {
      /* Exists, but we already converted it to an array */
      APR_ARRAY_PUSH( tmp_json->value.array, json_t * ) = json;
    }
    else {
      /* Standard insertion */
      apr_hash_set( obj->value.object, JSON_NAME( json ),
		    APR_HASH_KEY_STRING, json );
    }
    break;

  case JSON_ARRAY:
    APR_ARRAY_PUSH( obj->value.array, json_t * ) = json;
    break;

  default:
    json_writer_error( "values can only be added to arrays or objects" );
    break;
  }
}

/**
 * Start an object.
 * @param writer The json_writer object.
 */
void json_writer_object_start( void *writer_ptr )
{
  json_t *json;
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_object_start( writer->context ) ) {
    json_writer_error( "could not start object" );
    return;
  }

  json = json_object_create( writer->mp );
  json_add( writer, json );

  APR_ARRAY_PUSH( writer->json_stack, json_t * ) = json;
}

void json_writer_object_end( void *writer_ptr )
{
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_object_end( writer->context ) ) {
    json_writer_error( "could not end object" );
    return;
  }

  apr_array_pop( writer->json_stack );
}

void json_writer_array_start( void *writer_ptr )
{
  json_t *json;
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_array_start( writer->context ) ) {
    json_writer_error( "could not start array" );
    return;
  }

  json = json_array_create( writer->mp );
  json_add( writer, json );

  APR_ARRAY_PUSH( writer->json_stack, json_t * ) = json;
}

void json_writer_array_end( void *writer_ptr )
{
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_array_end( writer->context ) ) {
    json_writer_error( "could not end array" );
    return;
  }

  apr_array_pop( writer->json_stack );
}

/**
 * Save off a property.  Must be in an object for this to be valid.
 * @param writer The json_writer object.
 * @param name The name of the property to save.
 */
void json_writer_property_start( void *writer_ptr, unsigned char *name )
{
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_property_start( writer->context, name ) )
    json_writer_error( "could not start property \"%s\"", name );
}

void json_writer_property_end( void *writer_ptr )
{
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_property_end( writer->context ) )
    json_writer_error( "could not end property" );
}

void json_writer_string_write( void *writer_ptr, unsigned char *value )
{
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_can_write_value( writer->context ) ) {
    json_writer_error( "could not write string \"%s\"", value );
    return;
  }

  json_add( writer, json_string_create( writer->mp, value ) );
}

void json_writer_integer_write( void *writer_ptr, int value )
{
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_can_write_value( writer->context ) ) {
    json_writer_error( "could not write int \"%d\"", value );
    return;
  }

  json_add( writer, json_integer_create( writer->mp, value ) );
}

void json_writer_number_write( void *writer_ptr, double value )
{
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_can_write_value( writer->context ) ) {
    json_writer_error( "could not write number \"%lf\"", value );
    return;
  }

  json_add( writer, json_number_create( writer->mp, value ) );
}

void json_writer_boolean_write( void *writer_ptr, int value )
{
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_can_write_value( writer->context ) ) {
    json_writer_error( "could not write bool" );
    return;
  }

  json_add( writer, json_boolean_create( writer->mp, value ) );
}

void json_writer_null_write( void *writer_ptr )
{
  json_writer_t *writer = (json_writer_t *) writer_ptr;
  if ( !json_writer_ctx_can_write_value( writer->context ) ) {
    json_writer_error( "could not write null" );
    return;
  }

  json_add( writer, json_null_create( writer->mp ) );
}
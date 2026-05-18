#include <Python.h>
#include <pyport.h>

#include "apr_macros.h"
#include "json.h"
#include "json_writer.h"
#include "misc.h"
#include "xml2json.h"
#include "py_util.h"

/* Not sure what the best way to go about this is... */
#if !defined(HAVE_SSIZE_T) || SIZEOF_VOID_P != SIZEOF_SIZE_T
#define Py_ssize_t int
#endif

#ifndef Py_RETURN_TRUE
#define Py_RETURN_TRUE return Py_INCREF(Py_True), Py_True
#endif

#ifndef Py_RETURN_FALSE
#define Py_RETURN_FALSE return Py_INCREF(Py_False), Py_False
#endif

#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif

#ifndef PyDict_CheckExact
#define PyDict_CheckExact(op) ((op)->ob_type == &PyDict_Type)
#endif

static void py_dict_to_json( PyObject *obj, json_writer_t *writer );
static void py_list_to_json( PyObject *obj, json_writer_t *writer );
static void py_tuple_to_json( PyObject *obj, json_writer_t *writer );

static void py_variable_to_json_internal( PyObject *obj,
                                          json_writer_t *writer )
{
  if ( PyString_CheckExact( obj ) ) {
    json_writer_write_str( writer, PyString_AS_STRING( obj ) );
  }
  else if ( PyInt_CheckExact( obj ) ) {
    json_writer_write_integer( writer, PyInt_AS_LONG( obj ) );
  }
  else if ( PyFloat_CheckExact( obj ) ) {
    json_writer_write_number( writer, PyFloat_AS_DOUBLE( obj ) );
  }
  else if ( PyBool_Check( obj ) ) {
    json_writer_write_boolean( writer, ( obj == Py_True ) );
  }
  else if ( PyUnicode_CheckExact( obj ) ) {
    /* Create a new string object that is UTF-8 encoded. */
    /* PyUnicode_AS_UNICODE is deprecated in 3.3 and removed in 3.12 */
#if (PY_MAJOR_VERSION * 100) + PY_MINOR_VERSION >= 312
    PyObject *str_obj = PyUnicode_AsUTF8String( obj );
#else
    Py_UNICODE *unicode = PyUnicode_AS_UNICODE( obj );
    Py_ssize_t size = PyUnicode_GET_SIZE( obj );
    PyObject *str_obj = PyUnicode_EncodeUTF8( unicode, size, NULL );
#endif
    if ( str_obj != NULL ) {
      py_variable_to_json_internal( str_obj, writer );
      Py_DECREF( str_obj );
    }
  }
  else if ( PyDict_CheckExact( obj ) ) {
    py_dict_to_json( obj, writer );
  }
  else if ( PyList_CheckExact( obj ) ) {
    py_list_to_json( obj, writer );
  }
  else if ( PyTuple_CheckExact( obj ) ) {
    py_tuple_to_json( obj, writer ); 
  }
}

static void py_dict_to_json( PyObject *obj, json_writer_t *writer )
{
  PyObject *key, *value;
  Py_ssize_t pos = 0;

  json_writer_start_object( writer );

  while ( PyDict_Next( obj, &pos, &key, &value ) ) {
    json_writer_start_property( writer, PyString_AsString( key ) );
    py_variable_to_json_internal( value, writer );
    json_writer_end_property( writer );
  }

  json_writer_end_object( writer );
}

static void py_list_to_json( PyObject *obj, json_writer_t *writer )
{
  Py_ssize_t i;
  Py_ssize_t size = PyList_GET_SIZE( obj );
  PyObject *tmp;

  json_writer_start_array( writer );

  for ( i = 0; i < size; i++ ) {
    tmp = PyList_GET_ITEM( obj, i );
    py_variable_to_json_internal( tmp, writer );
  }

  json_writer_end_array( writer );
}

static void py_tuple_to_json( PyObject *obj, json_writer_t *writer )
{
  Py_ssize_t i;
  Py_ssize_t size = PyTuple_GET_SIZE( obj );
  PyObject *tmp;

  /*
   * There's not a good way to represent a tuple in JSON, we just make it an
   * array.
   */
  json_writer_start_array( writer );

  for ( i = 0 ; i < size; i++ ) {
    tmp = PyTuple_GET_ITEM( obj, i );
    py_variable_to_json_internal( tmp, writer );
  }

  json_writer_end_array( writer );
}


json_t *py_variable_to_json( apr_pool_t *mp, PyObject *obj )
{
  json_writer_t *writer;
  json_t *json = NULL;
  apr_pool_t *tmp_mp;

  if ( PyDict_CheckExact( obj ) || PyList_CheckExact( obj ) ||
       PyTuple_CheckExact( obj ) ) {
    apr_pool_create( &tmp_mp, NULL );
    writer = json_writer_create( tmp_mp, mp );
    py_variable_to_json_internal( obj, writer );
    json = writer->json;
    apr_pool_destroy( tmp_mp );
  }
  else {
    fprintf( stderr, "Must be an object, list or tuple.\n" );
  }

  return json;

}

PyObject *json_to_py_variable( json_t *json )
{
  json_t *tmp_json;
  apr_array_header_t *arr;
  apr_hash_index_t *idx;
  PyObject *py_list;
  PyObject *py_dict;
  Py_ssize_t i;

  switch ( json->type ) {
  case JSON_STRING:
    return PyString_FromString( (char *) json->value.string );
    break;

  case JSON_INTEGER:
    return PyInt_FromLong( json->value.integer );
    break;

  case JSON_NUMBER:
    return PyFloat_FromDouble( json->value.number );
    break;

  case JSON_OBJECT:
    py_dict = PyDict_New();
    for ( idx = apr_hash_first( NULL, json->value.object ); idx;
          idx = apr_hash_next( idx ) ) {
      apr_hash_this( idx, NULL, NULL, (void **) &tmp_json );
      PyDict_SetItemString( py_dict, (char *) JSON_NAME( tmp_json ),
                            json_to_py_variable( tmp_json ) );
    }
    return py_dict;
    break;

  case JSON_ARRAY:
    arr = json->value.array;
    py_list = PyList_New( arr->nelts );
    for ( i = 0; arr && i < arr->nelts; i++ ) {
      tmp_json = APR_ARRAY_IDX( arr, i, json_t * );
      PyList_SET_ITEM( py_list, i, json_to_py_variable( tmp_json ) );
    }
    return py_list;
    break;

  case JSON_BOOLEAN:
    if ( json->value.boolean ) {
      Py_RETURN_TRUE;
    }
    else {
      Py_RETURN_FALSE;
    }
    break;

  case JSON_NULL:
    Py_RETURN_NONE;
    break;
    
  default:
    return NULL;
    break;
  }
}

PyObject *xml_to_dict( const char *xml_file )
{
  apr_pool_t *tmp_mp;
  apr_file_t *file;
  json_t *json;
  PyObject *dict = NULL;

  apr_pool_create( &tmp_mp, NULL );
  apr_file_open( &file, xml_file, APR_READ | APR_BUFFERED, 0, tmp_mp );
  xml_to_json( tmp_mp, file, 1, &json );
  if ( json ) {
    dict = json_to_py_variable( json );
  }
  apr_pool_destroy( tmp_mp );
  
  if ( dict ) {
    return dict;
  }
  else {
    Py_RETURN_NONE;
  }
}

PyObject *json_to_dict( const char *json_file )
{
  apr_pool_t *tmp_mp;
  apr_file_t *in_file;
  json_t *json;
  PyObject *dict = NULL;
  parser_t *json_parser;

  apr_pool_create( &tmp_mp, NULL );
  json_parser = json_parser_create( tmp_mp );
  if ( open_apr_input_file( tmp_mp, json_file, &in_file ) &&
       json_parser_parse_file_to_obj( tmp_mp, json_parser, in_file,
                                      &json ) ) {
    dict = json_to_py_variable( json );
  }
  apr_pool_destroy( tmp_mp );
  
  if ( dict ) {
    return dict;
  }
  else {
    Py_RETURN_NONE;
  }
}

PyObject *verify_python_function( PyObject *func )
{
  return PyCallable_Check( func ) ? func : NULL;
}

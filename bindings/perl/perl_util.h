#ifndef PERL_UTIL_H
#define PERL_UTIL_H

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include "json.h"

json_t *perl_variable_to_json( apr_pool_t *mp, SV *input );
SV *json_to_perl_variable( json_t *json );
SV *xml_to_hash( const char *xml_file );
SV *json_to_hash( const char *json_file );
SV *verify_perl_function( SV *func );

#endif

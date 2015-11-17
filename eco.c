#include "eco.h"
#include "ecodb.h"
#include "log.h"

#include <string.h>

/******************************************************************/

const char* eco_name( const char* code )
{
	/* todo: bsearch() */
	for ( int i = 0; i < ecodb_cnt(); ++i ) {

		const EcoDbRecord* e = ecodb_get( i );
		if ( strcmp( code, e->eco ) == 0 ) {
			return e->name;
		}
	}

	return NULL;
}

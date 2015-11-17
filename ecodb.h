#ifndef __ecodb_h__
#define __ecodb_h__


#include <stdbool.h>


typedef struct {
	const char* eco;
	const char* name;
} EcoDbRecord;


int ecodb_cnt();
const EcoDbRecord* ecodb_get( int index );


#endif /* __ecodb_h__ */

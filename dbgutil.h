#ifndef __dbgutil_h__
#define __dbgutil_h__

#ifndef NDEBUG

#include "log.h"
#include <stdlib.h>

#define dbgutil_test( test ) \
	if ( !(test) ) { \
		LOG( ERROR, "Debug Test Failed: %s File: %s Line: %d", __STRING( test ), __FILE__, __LINE__ ); \
		abort(); \
	}

#else

#define dbgutil_test( test )

#endif // NDEBUG

#endif // __dbgutil_h__

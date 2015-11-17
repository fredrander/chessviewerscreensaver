#ifndef __cmdline_h__
#define __cmdline_h__


#include <stdbool.h>


typedef struct
{
	const char* pgnfile;
	const char* engine;
	const char* movespeed_s;
	const char* enginetime_percentage;
	bool random_order;
} CmdLineOptions;


bool cmdline_parse( int argc, char* argv[], CmdLineOptions* options );

#endif // __cmdline_h__

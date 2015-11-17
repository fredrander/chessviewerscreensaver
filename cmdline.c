#include "cmdline.h"
#include "log.h"
#include "dbgutil.h"

#include <stdlib.h>
#include <string.h>

/**********************************************************************/

typedef enum
{
	CMD_LINE_PARSE_IDLE,
	CMD_LINE_PARSE_PGN_FILE,
	CMD_LINE_PARSE_MOVE_SPEED,
	CMD_LINE_PARSE_ENGINE,
	CMD_LINE_PARSE_ENGINE_TIME_PERCENTAGE
} CmdLineParseState;

/**********************************************************************/

bool cmdline_parse( int argc, char* argv[], CmdLineOptions* options )
{
	dbgutil_test( options != NULL );

	memset( options, 0, sizeof(CmdLineOptions) );

	CmdLineParseState state = CMD_LINE_PARSE_IDLE;

	for ( int i = 1; i < argc; ++i ) {

		LOG( INFO, "Cmd line: %s", argv[i] );

		switch ( state ) {
		case CMD_LINE_PARSE_IDLE:

			if ( strcmp( argv[i], "--pgnfile" ) == 0 ||
				strcmp( argv[i], "-f" ) == 0 ) {

				state = CMD_LINE_PARSE_PGN_FILE;
			} else if ( strcmp( argv[i], "--engine" ) == 0 ||
				strcmp( argv[i], "-e" ) == 0 ) {

				state = CMD_LINE_PARSE_ENGINE;
			} else if ( strcmp( argv[i], "--speed" ) == 0 ||
				strcmp( argv[i], "-s" ) == 0 ) {

				state = CMD_LINE_PARSE_MOVE_SPEED;
			} else if ( strcmp( argv[i], "--engine-time" ) == 0 ||
				strcmp( argv[i], "-t" ) == 0 ) {

				state = CMD_LINE_PARSE_ENGINE_TIME_PERCENTAGE;
			} else if ( strcmp( argv[i], "--random-order" ) == 0 ||
				strcmp( argv[i], "-r" ) == 0 ) {

				options->random_order = true;
			} else {
				return false;
			}
			break;

		case CMD_LINE_PARSE_PGN_FILE:
			options->pgnfile = argv[ i ];
			state = CMD_LINE_PARSE_IDLE;
			break;

		case CMD_LINE_PARSE_ENGINE:
			options->engine = argv[ i ];
			state = CMD_LINE_PARSE_IDLE;
			break;

		case CMD_LINE_PARSE_MOVE_SPEED:
			options->movespeed_s = argv[ i ];
			state = CMD_LINE_PARSE_IDLE;
			break;

		case CMD_LINE_PARSE_ENGINE_TIME_PERCENTAGE:
			options->enginetime_percentage = argv[ i ];
			state = CMD_LINE_PARSE_IDLE;
			break;
		}
	}

	return true;
}

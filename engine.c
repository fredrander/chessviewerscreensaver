#include "engine.h"
#include "popen2.h"
#include "log.h"

#ifndef __USE_BSD
#define __USE_BSD /* for usleep */
#endif /* __USE_BSD */

#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>


/************************************************************/

typedef enum
{
	ENGINE_COLOR_WHITE,
	ENGINE_COLOR_BLACK
} EngineColorType;

/************************************************************/

#define ENGINE_CMD_INIT_REQ "uci"
#define ENGINE_CMD_INIT_RSP "uciok"
#define ENGINE_CMD_READY_REQ "isready"
#define ENGINE_CMD_READY_RSP "readyok"
#define ENGINE_CMD_QUIT_REQ "quit"
#define ENGINE_CMD_NEW_GAME_REQ "ucinewgame"
#define ENGINE_CMD_GO_INFINITE_REQ "go infinite"
#define ENGINE_CMD_GO_TIMED_REQ "go movetime "
#define ENGINE_CMD_STOP_REQ "stop"
#define ENGINE_CMD_OPTION_ANALYSE "setoption name UCI_AnalyseMode value true"

#define ENGINE_POSITION_CMD_ALLOC_SIZE (16 * 1024)

#define ENGINE_PARSE_SCORE_STR " score "
#define ENGINE_PARSE_SCORE_CP_STR "cp "
#define ENGINE_PARSE_SCORE_MATE_STR "mate "
#define ENGINE_PARSE_LINE_STR " pv "
#define ENGINE_PARSE_DEPTH_STR " depth "

/***********************************************************/

static bool engine_init_done = false;
static char* engine_position_cmd = NULL;
static size_t engine_position_cmd_size = 0;
static EngineColorType engine_color = ENGINE_COLOR_WHITE;

static struct popen2 engine_child;
static pthread_t engine_thread_info;
static bool engine_thread_stop = false;
static bool engine_thread_idle_req = false;
static bool engine_thread_idle = true;
static engine_cb_func engine_cb = NULL;
static void* engine_user_data = NULL;

/***********************************************************/

static void engine_send_cmd( const char* cmd );
static bool engine_read_rsp( const char* rsp );
static bool engine_send_cmd_and_get_rsp( const char* cmd, const char* rsp );
static bool engine_data_avail( uint32_t to );
static bool engine_write_ready( uint32_t to );
static void engine_add_to_position_cmd( const char* data );
static bool engine_parse_line_and_notify_listener( const char* line );
static bool engine_read_line( char* line, size_t maxlen );
static void* engine_thread( void* arg );
static bool engine_start_thread();
static bool engine_stop_thread();
static EngineColorType engine_get_starting_color(const char* startFEN);

/***********************************************************/

bool engine_init( const char* bin )
{
	engine_child.child_pid = -1;
	engine_child.from_child = -1;
	engine_child.to_child = -1;
	if ( bin == NULL ) {
		return false;
	}
	if ( strlen( bin ) < 1 ) {
		return false;
	}
	if ( !popen2( bin, &engine_child ) ) {
		return false;
	}

	if ( !engine_send_cmd_and_get_rsp( ENGINE_CMD_INIT_REQ, ENGINE_CMD_INIT_RSP ) ) {
		return false;
	}
	if ( !engine_send_cmd_and_get_rsp( ENGINE_CMD_READY_REQ, ENGINE_CMD_READY_RSP ) ) {
		return false;
	}
	engine_send_cmd( ENGINE_CMD_OPTION_ANALYSE );

	if ( !engine_start_thread() ) {
		return false;
	}

	engine_init_done = true;
	return true;
}

void engine_close()
{
	if ( engine_child.child_pid >= 0 ) {
		engine_send_cmd( ENGINE_CMD_QUIT_REQ );
		waitpid( engine_child.child_pid, NULL, 0 );
		popen2_close( &engine_child );
	}

	if ( engine_init_done ) {
		engine_stop_thread();
	}

	if ( engine_position_cmd != NULL ) {
		free( engine_position_cmd );
		engine_position_cmd = NULL;
		engine_position_cmd_size = 0;
	}

	engine_init_done = false;
}

void engine_new_game(const char* startFEN)
{
	if ( !engine_init_done ) {
		return;
	}

	engine_send_cmd( ENGINE_CMD_NEW_GAME_REQ );

	/* set starting color */
	engine_color = engine_get_starting_color( startFEN );

	if ( engine_position_cmd != NULL ) {
		engine_position_cmd[ 0 ] = '\0';
	}

	engine_add_to_position_cmd( "position " );
	if ( startFEN != NULL ) {
		engine_add_to_position_cmd( "fen " );
		engine_add_to_position_cmd( startFEN );
		engine_add_to_position_cmd( " moves" );
	} else {
		engine_add_to_position_cmd( "startpos moves" );
	}
	engine_send_cmd( engine_position_cmd );
}

void engine_add_move( const char* long_algebraic )
{
	if ( !engine_init_done ) {
		return;
	}

	engine_add_to_position_cmd( " " );
	engine_add_to_position_cmd( long_algebraic );

	engine_send_cmd( engine_position_cmd );

	/* toggle color */
	if ( engine_color == ENGINE_COLOR_WHITE ) {
		engine_color = ENGINE_COLOR_BLACK;
	} else {
		engine_color = ENGINE_COLOR_WHITE;
	}
}

void engine_go( int time_ms, engine_cb_func cb, void* user_data )
{
	if ( !engine_init_done ) {
		return;
	}

	engine_user_data = user_data;
	engine_cb = cb;
	engine_thread_idle_req = false;

	if ( time_ms < 0 ) {
		engine_send_cmd( ENGINE_CMD_GO_INFINITE_REQ );
	} else if ( time_ms > 0 ) {
		int len = strlen(ENGINE_CMD_GO_TIMED_REQ) + 16;
		char* tmpstr = malloc( len );
		snprintf( tmpstr, len, "%s%d", ENGINE_CMD_GO_TIMED_REQ, time_ms );
		engine_send_cmd( tmpstr );
		free( tmpstr );
	}
}

void engine_stop()
{
	if ( !engine_init_done ) {
		return;
	}

	engine_send_cmd( ENGINE_CMD_STOP_REQ );

	engine_thread_idle_req = true;
	while ( !engine_thread_idle ) {
		usleep( 1 * 1000 );
	}
	engine_user_data = NULL;
	engine_cb = NULL;
}

/***********************************************************/

static void engine_send_cmd( const char* cmd )
{
	if ( !engine_write_ready(300) ) {

		LOG( WARNING, "Engine is not ready to receive input");
		return;
	}

	write( engine_child.to_child, cmd, strlen(cmd) );
	write( engine_child.to_child, "\n", 1 );
	LOG(DEBUG, "Engine cmd: %s", cmd);
}

static bool engine_read_rsp( const char* rsp )
{
	const char* fpos = rsp;

	while ( *fpos ) {

		char c = '\0';
		if ( read( engine_child.from_child, &c, sizeof(char) ) > 0 ) {
			if ( c == *fpos ) {
				++fpos;
			} else {
				fpos = rsp;
			}
		}
	}

	LOG(DEBUG, "Engine response: %s", rsp);

	return true;
}

static bool engine_send_cmd_and_get_rsp( const char* cmd, const char* rsp )
{
	engine_send_cmd( cmd );
	return engine_read_rsp( rsp );
}

static bool engine_data_avail( uint32_t to )
{
	fd_set fds;
	FD_ZERO( &fds );
	FD_SET( engine_child.from_child, &fds );

	struct timeval timeout;
	timeout.tv_sec = to / 1000;
	timeout.tv_usec = 1000 * ( to % 1000 );

	int sres = select( engine_child.from_child + 1, &fds, NULL, NULL, &timeout);

	if ( sres != 1 ) {
		return false;
	}

	return true;
}

static bool engine_write_ready( uint32_t to )
{
	fd_set fds;
	FD_ZERO( &fds );
	FD_SET( engine_child.to_child, &fds );

	struct timeval timeout;
	timeout.tv_sec = to / 1000;
	timeout.tv_usec = 1000 * ( to % 1000 );

	int sres = select( engine_child.to_child + 1, NULL, &fds, NULL, &timeout);

	if ( sres != 1 ) {
		return false;
	}

	return true;
}

static void engine_add_to_position_cmd( const char* data )
{
	size_t addlen = strlen( data );
	size_t existinglen = (engine_position_cmd == NULL) ?
				0 : strlen(engine_position_cmd);

	bool moremem = false;
	while ( existinglen + addlen + 1 > engine_position_cmd_size ) {
		engine_position_cmd_size += ENGINE_POSITION_CMD_ALLOC_SIZE;
		moremem = true;
	}

	if ( moremem ) {
		bool was_empty = ( engine_position_cmd == NULL );
		engine_position_cmd = realloc( engine_position_cmd,
				engine_position_cmd_size );

		if ( was_empty ) {
			engine_position_cmd[ 0 ] = '\0';
		}
	}

	strcat( engine_position_cmd, data );
}

static bool engine_read_line( char* line, size_t maxlen )
{
	if ( maxlen < 1 ) {
		return false;
	}

	size_t linelen = 0;
	line[ 0 ] = '\0';

	char c = '\0';
	while ( c != '\n' ) {
		read( engine_child.from_child, &c, 1 );
		if ( c != '\n' && linelen < (maxlen - 1) ) {
			line[ linelen ] = c;
			line[ linelen + 1 ] = '\0';
			linelen++;
		}
	}

	return true;
}

static bool engine_parse_line_and_notify_listener( const char* line )
{
	if ( engine_cb == NULL ) {
		return true;
	}

	const char* pline = strstr( line, ENGINE_PARSE_LINE_STR );
	if ( pline == NULL ) {
		return false;
	}

	pline += strlen( ENGINE_PARSE_LINE_STR );

	const char* pscore = strstr( line, ENGINE_PARSE_SCORE_STR );
	if ( pscore == NULL ) {
		return false;
	}

	pscore += strlen( ENGINE_PARSE_SCORE_STR );
	int score = 0;
	EngineScoreType type = ENGINE_SCORE_CENTI_PAWN;

	if ( strstr(pscore, ENGINE_PARSE_SCORE_CP_STR) == pscore ) {

		pscore += strlen( ENGINE_PARSE_SCORE_CP_STR );
		score = atoi( pscore );
		type = ENGINE_SCORE_CENTI_PAWN;

	} else if ( strstr(pscore, ENGINE_PARSE_SCORE_MATE_STR) == pscore ) {

		pscore += strlen( ENGINE_PARSE_SCORE_MATE_STR );
		score = atoi( pscore );
		type = ENGINE_SCORE_MATE;
	}

	if ( engine_color == ENGINE_COLOR_BLACK ) {
		score = -1 * score;
	}

	const char* pdepth = strstr( line, ENGINE_PARSE_DEPTH_STR );
	if ( pdepth == NULL ) {
		return false;
	}

	pdepth += strlen( ENGINE_PARSE_DEPTH_STR );
	int depth = atoi( pdepth );

	if ( engine_cb != NULL ) {

		engine_cb( type, score, depth, pline, engine_user_data );
	}
	return true;
}

static void* engine_thread( void* arg )
{
	while ( !engine_thread_stop ) {

		if ( !engine_thread_idle_req ) {

			if ( engine_data_avail(10) ) {

				const size_t MAX_LINE = 1024;
				char line[ MAX_LINE ];
				engine_read_line( line, MAX_LINE );

				engine_parse_line_and_notify_listener( line );
			}

		} else {
			if ( !engine_thread_idle ) {
				engine_thread_idle = true;
			}
			usleep( 1 * 1000 );
		}
	}

	return NULL;
}

static bool engine_start_thread()
{
	int pres = pthread_create( &engine_thread_info, NULL,
			engine_thread, NULL);

	return pres == 0;
}

static bool engine_stop_thread()
{
	engine_thread_stop = true;
	pthread_join( engine_thread_info, NULL );
	return true;
}

static EngineColorType engine_get_starting_color(const char* startFEN)
{
	EngineColorType result = ENGINE_COLOR_WHITE;
	if ( startFEN != NULL ) {
		const char* p = strchr( startFEN, ' ' );
		if ( p != NULL && p < ( startFEN + strlen(startFEN) - 1 ) ) {
			p++;
			if (*p == 'w' || *p == 'W' ) {
				result = ENGINE_COLOR_WHITE;
			} else {
				result = ENGINE_COLOR_BLACK;
			}
		}
	}

	return result;
}

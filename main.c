#include "ui.h"
#include "pgn.h"
#include "eco.h"
#include "engine.h"
#include "defs.h"
#include "log.h"
#include "cmdline.h"
#include "dbgutil.h"

#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

/**************************************************************************/

#define PRE_GAME_DELAY_S (1 * movetime_s)
#define POST_GAME_DELAY_S ( (8 * movetime_s) > 30 ? 30 : (8 * movetime_s) )

/**************************************************************************/

typedef struct
{
	Position pos;
	int movenum;
	Color col;
} EngineMoveInfo;

/***********************************************************************/

static bool next_game( bool random );
static void update_engine_move_info( EngineMoveInfo* moveinfo,
		const Position* p, int next_movenum, Color next_color );
static void redraw_board( const Position* p );
static void engine_callback( EngineScoreType type, int score,
		int depth, const char* line_str, void* moveinfo );
static void signal_handler( int signal );

/**********************************************************************/

int main(int argc, char* argv[])
{
	log_init();

	/* install signal handler for some exceptions */
	(void) signal( SIGSEGV, signal_handler );
	(void) signal( SIGPIPE, signal_handler );
	(void) signal( SIGSTKFLT, signal_handler );
	(void) signal( SIGBUS, signal_handler );
	(void) signal( SIGCHLD, signal_handler );
	(void) signal( SIGFPE, signal_handler );
	(void) signal( SIGABRT, signal_handler );

	CmdLineOptions cmdline;
	if ( !cmdline_parse( argc, argv, &cmdline ) ) {
		log_close();
		return 1;
	}

	int movetime_s = 2;
	if ( cmdline.movespeed_s != NULL ) {
		movetime_s = atoi( cmdline.movespeed_s );
	}

	int enginetime_percentage = 20;
	int enginetime_ms = ( enginetime_percentage * ( movetime_s * 1000 ) ) / 100;
	if ( cmdline.enginetime_percentage != NULL ) {
		enginetime_percentage = atoi( cmdline.enginetime_percentage );
		enginetime_ms = ( enginetime_percentage * ( movetime_s * 1000 ) ) / 100;
	}

	if (!pgn_init( cmdline.pgnfile )) {
		log_close();
		return 2;
	}

	if ( engine_init( cmdline.engine ) ) {
		LOG( INFO, "Engine: %s", cmdline.engine );
	} else {
		LOG( INFO, "No engine" );
	}

	if (!ui_init()) {
		pgn_close();
		log_close();
		return 3;
	}

	while ( next_game( cmdline.random_order ) ) {
		/* new game, get game info an draw initial board */
		const GameInfo* info = pgn_game_info();
		const Position* p = pgn_position();
		EngineMoveInfo moveinfo;

		LOG( INFO, "Start new game, %s vs. %s",
			(info != NULL && info->white != NULL) ? info->white : "<Unknown>",
			(info != NULL && info->black != NULL) ? info->black : "<Unknown>");
		LOG( INFO, "Event: %s",
			(info != NULL && info->event != NULL) ? info->event : "<Unknown>");
		LOG( INFO, "Round: %s",
			(info != NULL && info->round != NULL) ? info->round : "<Unknown>");
		LOG( INFO, "Site: %s",
			(info != NULL && info->site != NULL) ? info->site : "<Unknown>");
		LOG( INFO, "Date: %s",
			(info != NULL) ? info->datestr : "<Unknown>");

		ui_clear();
		redraw_board(p);

		if ( NULL != info) {
			const char* ecoinfo = eco_name( info->eco );

			ui_draw_game_info( info->white, info->black, info->whiteelo,
					info->blackelo, info->event, info->round,
					info->site, info->datestr, info->eco, ecoinfo);

			engine_new_game( info->fen );
		} else {
			engine_new_game( NULL );
		}

		if ( NULL != p) {
			ui_flush();

			update_engine_move_info( &moveinfo, p, 1, pgn_next_to_move() );
			engine_go( enginetime_ms, engine_callback, &moveinfo );

			sleep( PRE_GAME_DELAY_S );
			engine_stop();

			const Move* m = pgn_next_move();

			while ( NULL != m) {

				redraw_board(p);

				ui_highlight_move(m->from, m->to);
				ui_draw_move_str(m->movenum, isupper(m->piece), m->movestr);
				ui_flush();

				int next_movenum = m->movenum;
				if ( pgn_next_to_move() == WHITE ) {
					next_movenum = moveinfo.movenum + 1;
				}
				update_engine_move_info( &moveinfo, p, next_movenum, pgn_next_to_move() );
				engine_add_move( m->long_algebraic );
				engine_go( enginetime_ms, engine_callback, &moveinfo );

				sleep( movetime_s );
				engine_stop();

				m = pgn_next_move();
			}

			if ( NULL != info) {
				switch ( info->result ) {
				case WHITE_WIN:
					ui_draw_result( "White wins" );
					break;
				case BLACK_WIN:
					ui_draw_result( "Black wins" );
					break;
				case DRAW:
					ui_draw_result( "Draw" );
					break;
				case UNKNOWN:
					ui_draw_result( "Game ended" );
					break;
				}
			} else {
				ui_draw_result( "Game ended" );
			}

			ui_flush();

			engine_stop();
			int engine_time_post_game_ms = ( 1000 * enginetime_percentage * POST_GAME_DELAY_S ) / 100;
			engine_go( engine_time_post_game_ms, engine_callback, &moveinfo );
			sleep( POST_GAME_DELAY_S );
			engine_stop();

		} else {
			ui_flush();
		}

		ui_toggle_board_position();
	}

	pgn_close();
	engine_close();
	ui_close();
	log_close();

	return 0;
}

/**********************************************************************/
static bool next_game( bool random )
{
	if ( random ) {
		return pgn_next_random_game();
	} else {
		return pgn_next_game();
	}
}

static void update_engine_move_info( EngineMoveInfo* moveinfo,
		const Position* p, int next_movenum, Color next_color )
{
	dbgutil_test( p != NULL );

	memcpy( &(moveinfo->pos), p, sizeof( Position ) );

	if ( next_movenum > 0 ) {
		moveinfo->movenum = next_movenum;
	}
	moveinfo->col = next_color;
}

static void redraw_board(const Position* p)
{
	ui_draw_board();
	if ( NULL != p) {
		for (int pos = 0; CW_NB_OF_SQUARES > pos; ++pos) {
			if (p->board[pos] != ' ') {
				ui_draw_piece(p->board[pos], pos);
			}
		}
	}
}

static void engine_callback( EngineScoreType type, int score,
		int depth, const char* line_str, void* moveinfo )
{
	dbgutil_test( moveinfo != NULL );

	LOG( DEBUG, "Engine score: %d Depth: %d", score, depth );
	LOG( DEBUG, "Engine line: %s", line_str );

	EngineMoveInfo info;
	memcpy( &info, moveinfo, sizeof(EngineMoveInfo) );

	const size_t MAX_EVAL_STR = 128;
	char pgnlinestr[ MAX_EVAL_STR ];
	pgnlinestr[ 0 ] = '\0';

	bool ok = true;
	const char* startp = line_str;
	const char* endp = NULL;
	do {
		endp = strchr( startp, ' ' );
		char long_algebraic[ CW_MAX_LONG_ALGEBRAIC_STRING ];

		size_t len = 0;
		if ( endp != NULL ) {
			len = endp - startp;
		} else {
			len = strlen( line_str ) - (startp - line_str);
		}
		if ( len < CW_MAX_LONG_ALGEBRAIC_STRING ) {
			strncpy( long_algebraic, startp, len );
			long_algebraic[ len ] = '\0';

			char pgnstr[ CW_MAX_MOVE_STRING ];

			ok = pgn_long_algebraic_to_pgn( long_algebraic, pgnstr, &(info.pos) );
			ok = ok && pgn_long_algebraic_perform_move( long_algebraic, &(info.pos) );

			if ( ok ) {

				if ( strlen(pgnlinestr) > 0 ) {
					strcat( pgnlinestr, " " );
				}
				if ( info.col == WHITE ) {
					char numstr[ 16 ];
					sprintf( numstr, "%d. ", info.movenum );
					strcat( pgnlinestr, numstr );
				} else if ( strlen(pgnlinestr) < 1 ) {
					char numstr[ 16 ];
					sprintf( numstr, "%d... ", info.movenum );
					strcat( pgnlinestr, numstr );
				}

				if ( info.col == WHITE ) {
					info.col = BLACK;
				} else {
					info.movenum++;
					info.col = WHITE;
				}
				strcat( pgnlinestr, pgnstr );
			}
		}

		startp = endp + 1;

	} while ( ok && (endp != NULL) && (strlen( pgnlinestr ) < (MAX_EVAL_STR - CW_MAX_MOVE_STRING - 1 - 16)) );

	LOG( DEBUG, "Engine line: %s", pgnlinestr);

	char scorestr[ 32 ];
	if ( type == ENGINE_SCORE_CENTI_PAWN ) {
		if ( score < 0 ) {
			scorestr[ 0 ] = '-';
		} else {
			scorestr[ 0 ] = '+';
		}
		snprintf( scorestr + 1, 31, "%0.2lf", (double) abs(score) / 100.0);
	} else {
		if ( score < 0 ) {
			snprintf( scorestr, 32, "Black Mates in %d", abs(score) );
		} else {
			snprintf( scorestr, 32, "White Mates in %d", abs(score) );
		}
	}

	ui_draw_engine_eval( scorestr, pgnlinestr );
	ui_flush();
}

static void signal_handler( int signal )
{
	LOG( ERROR, "Caught signal %d", signal );
}

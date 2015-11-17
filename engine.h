#ifndef __engine_h__
#define __engine_h__


#include <stdbool.h>


bool engine_init( const char* bin );
void engine_close();

/* startFEN == NULL means normal start position */
void engine_new_game( const char* startFEN );

void engine_add_move( const char* long_algebraic );

typedef enum
{
	ENGINE_SCORE_CENTI_PAWN,
	ENGINE_SCORE_MATE
} EngineScoreType;

typedef void (*engine_cb_func)( EngineScoreType score_type, int score,
		int depth, const char* best_line_str, void* user_data );
void engine_go( int time_ms, engine_cb_func cb, void* user_data );
void engine_stop();

#endif /* __engine_h__ */

#ifndef __pgn_h__
#define __pgn_h__

#include "defs.h"

#include <stdbool.h>

typedef struct
{
	char board[CW_NB_OF_SQUARES];
} Position;

typedef enum
{
	NORMAL,
	CAPTURE,
	CASTLE,
	EN_PASSANT,
	PROMOTE
} MoveType;


typedef struct
{
	MoveType type;

	char piece;

	int from;
	int to;

	int movenum;
	char movestr[CW_MAX_MOVE_STRING];

	char long_algebraic[CW_MAX_LONG_ALGEBRAIC_STRING];

	/* only valid if type == CAPTURE or type == PROMOTE */
	char capturepiece;

	/* only valid if type == CASTLE */
	char castlerookpiece;

	int castlerookfrom;
	int castlerookto;

	/* only valid if type == EN_PASSANT */
	int enpassantcapturepos;

	/* only valid if type == PROMOTE */
	char promotepiece;

} Move;

typedef enum
{
	UNKNOWN,
	WHITE_WIN,
	BLACK_WIN,
	DRAW
} GameResultType;

#define PGN_MAX_LEN_DATE 11
#define PGN_MAX_LEN_ECO 4
#define PGN_MAX_LEN_ELO 5

typedef struct
{
	char* white;
	char* black;
	char whiteelo[PGN_MAX_LEN_ELO];
	char blackelo[PGN_MAX_LEN_ELO];
	char* event;
	char* round;
	char* site;
	char datestr[PGN_MAX_LEN_DATE];
	char eco[PGN_MAX_LEN_ECO];
	char* fen;
	GameResultType result;
} GameInfo;

bool pgn_init(const char* filename);
void pgn_close();

bool pgn_next_game();
bool pgn_next_random_game();

const Position* pgn_position();
const Move* pgn_next_move();

typedef enum
{
	WHITE,
	BLACK
} Color;
Color pgn_next_to_move();

const GameInfo* pgn_game_info();

bool pgn_long_algebraic_to_pgn( char* long_algebraic, char* pgn, Position* pos );
bool pgn_long_algebraic_perform_move( char* long_algebraic, Position* pos );

#endif /* __pgn_h__ */

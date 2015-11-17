#include "chess.h"
#include "defs.h"
#include "dbgutil.h"

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/************************************************************************/

#define CHESS_FILE( p ) (p & 7)
#define CHESS_RANK( p ) (p >> 3)

#define CHESS_VERTICAL_OR_HORIZONTAL_MOVE( from, to ) ( ( CHESS_FILE( from ) == CHESS_FILE( to ) ) || \
							( CHESS_RANK( from ) == CHESS_RANK( to ) ) )

#define CHESS_DIAGONAL_MOVE( from, to ) ( abs( CHESS_FILE( from ) - CHESS_FILE( to ) ) == \
					abs( CHESS_RANK( from ) - CHESS_RANK( to ) ) )

#define CHESS_FILE_DISTANCE( from, to ) ( abs( CHESS_FILE( from ) - CHESS_FILE( to ) ) )

#define CHESS_RANK_DISTANCE( from, to ) ( abs( CHESS_RANK( from ) - CHESS_RANK( to ) ) )

/************************************************************************/

static bool chess_is_move_blocked(const char* board, int from, int to);
static void chess_perform_move( char* board, int from, int to );

/************************************************************************/

/*
   +----+----+----+----+----+----+----+----+
 8 | 56 | 57 | 58 | 59 | 60 | 61 | 62 | 63 |
   +----+----+----+----+----+----+----+----+
 7 | 48 | 49 | 50 | 51 | 52 | 53 | 54 | 55 |
   +----+----+----+----+----+----+----+----+
 6 | 40 | 41 | 42 | 43 | 44 | 45 | 46 | 47 |
   +----+----+----+----+----+----+----+----+
 5 | 32 | 33 | 34 | 35 | 36 | 37 | 38 | 39 |
   +----+----+----+----+----+----+----+----+
 4 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 |
   +----+----+----+----+----+----+----+----+
 3 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 |
   +----+----+----+----+----+----+----+----+
 2 | 8  | 9  | 10 | 11 | 12 | 13 | 14 | 15 |
   +----+----+----+----+----+----+----+----+
 1 | 0  | 1  | 2  | 3  | 4  | 5  | 6  | 7  |
   +----+----+----+----+----+----+----+----+
     a    b    c    d    e    f    g    h
 */

bool chess_is_possible_move(const char* board, int from, int to, char piece, bool capture)
{
	bool ok = false;

	switch (piece) {
	case 'K': /* white king */
		ok = (CHESS_FILE_DISTANCE( from, to ) < 2 && CHESS_RANK_DISTANCE( from, to ) < 2);
		ok = ok || (( (from == 4 && to == 2) || (from == 4 && to == 6) ) && !capture);
		break;
	case 'k':
		ok = (CHESS_FILE_DISTANCE( from, to ) < 2 && CHESS_RANK_DISTANCE( from, to ) < 2);
		ok = ok || (( (from == 60 && to == 58) || (from == 60 && to == 62) ) && !capture);
		break;

	case 'Q':
	case 'q':
		ok = CHESS_VERTICAL_OR_HORIZONTAL_MOVE( from, to ) || CHESS_DIAGONAL_MOVE(from, to);
		break;

	case 'R':
	case 'r':
		ok = CHESS_VERTICAL_OR_HORIZONTAL_MOVE(from, to);
		break;

	case 'B':
	case 'b':
		ok = CHESS_DIAGONAL_MOVE(from, to);
		break;

	case 'N':
	case 'n':
		ok = (2 == CHESS_FILE_DISTANCE(from, to) && 1 == CHESS_RANK_DISTANCE(from, to)) ||
			(1 == CHESS_FILE_DISTANCE(from, to) && 2 == CHESS_RANK_DISTANCE(from, to));
		break;

	case 'P': /* white pawn */
		ok = from < to;
		if (!capture) {
			ok = ok &&
				0 == CHESS_FILE_DISTANCE(from, to) &&
				(1 == CHESS_RANK_DISTANCE(from, to) ||
				(2 == CHESS_RANK_DISTANCE(from, to) && 1 == CHESS_RANK(from)));
		} else {
			ok = ok &&
				1 == CHESS_RANK_DISTANCE(from, to) && 1 == CHESS_FILE_DISTANCE(from, to);
		}
		break;

	case 'p':
		ok = from > to;
		if (!capture) {
			ok = ok &&
				0 == CHESS_FILE_DISTANCE(from, to) &&
				(1 == CHESS_RANK_DISTANCE(from, to) ||
				(2 == CHESS_RANK_DISTANCE(from, to) && 6 == CHESS_RANK(from)));
		} else {
			ok = ok &&
				1 == CHESS_RANK_DISTANCE(from, to) && 1 == CHESS_FILE_DISTANCE(from, to);
		}
		break;
	}

	/* knights can fly, other pieces can be blocked */
	if (ok && 'N' != piece && 'n' != piece) {
		ok = !chess_is_move_blocked(board, from, to);
	}

	return ok;
}

bool chess_is_in_check(const char* board, char kingpiece)
{
	/* find king */
	int to = -1;
	for (int i = 0; 0 > to && i < CW_NB_OF_SQUARES; ++i) {
		if (board[i] == kingpiece) {
			to = i;
		}
	}

	dbgutil_test(0 <= to); /* there SHOULD be a king */

	bool whiteking = isupper(kingpiece);

	for (int i = 0; i < CW_NB_OF_SQUARES; ++i) {
		if (CW_NO_PIECE != board[i]) {
			if ( (whiteking && islower(board[i])) || (!whiteking && isupper(board[i])) ) {
				if ( chess_is_possible_move(board, i, to, board[i], true) ) {
					return true;
				}
			}
		}
	}

	return false;
}

bool chess_is_mated(const char* board, char kingpiece)
{
	bool kingwhite = (isupper( kingpiece ) != 0);
	for ( int i = 0; i < CW_NB_OF_SQUARES; ++i ) {

		bool fromwhite = (isupper( board[i] ) != 0);
		if ( board[i] != CW_NO_PIECE && fromwhite == kingwhite ) {

			for ( int j = 0; j < CW_NB_OF_SQUARES; ++j ) {

				bool towhite = (isupper( board[j] ) != 0);
				bool capture = ( board[j] != CW_NO_PIECE && towhite != kingwhite );
				if ( capture || board[j] == CW_NO_PIECE ) {

					if ( chess_is_possible_move( board, i, j, board[ i ], capture) &&
						!chess_is_castling( board, board[ i ], i, j, NULL, NULL)) {

						char board_copy[ CW_NB_OF_SQUARES ];
						memcpy( board_copy, board, CW_NB_OF_SQUARES );
						chess_perform_move( board_copy, i, j );
						if ( !chess_is_in_check( board_copy, kingpiece ) ) {
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

bool chess_is_en_passant_capture(const char* board, char piece, int from, int to)
{
	dbgutil_test(NULL != board);

	if ('P' != piece && 'p' != piece) {
		/* not even a pawn move, ...come on */
		return false;
	}

	if (1 != CHESS_RANK_DISTANCE(from, to)) {
		/* you really should go forward */
		return false;
	}

	if (1 != CHESS_FILE_DISTANCE(from, to)) {
		/* en passant means a pawn take is involved */
		return false;
	}

	if ( CW_NO_PIECE != board[to] ) {
		/* did take something else on destination */
		return false;
	}

	if ('P' == piece) {
		/* white pawn */
		if (4 != CHESS_RANK(from)) {
			/* not fifth rank */
			return false;
		}

		if ('p' != board[to - 8]) {
			/* hmm, no black pawn here */
			return false;
		}
	} else if ('p' == piece) {
		/* black pawn */
		if (3 != CHESS_RANK(from)) {
			/* not from fourth rank */
			return false;
		}

		if ('P' != board[to + 8]) {
			return false;
		}

	}

	return true;
}

bool chess_is_castling(const char* board, char piece, int from, int to,
		int* rookfrom, int* rookto)
{
	if ( piece != 'k' && piece != 'K' ) {
		return false;
	}

	int ffrom = CHESS_FILE( from );
	if ( ffrom != 4 ) {
		return false;
	}

	int fto = CHESS_FILE( to );
	if ( fto != 2 && fto != 6 ) {
		return false;
	}

	if ( rookfrom != NULL && rookto != NULL ) {

		if ( fto == 6 ) {
			*rookfrom = to + 1;
			*rookto = to - 1;
		} else {
			*rookfrom = to - 2;
			*rookto = to + 1;
		}
	}

	return true;
}

/************************************************************************/

static bool chess_is_move_blocked(const char* board, int from, int to)
{
	int xdiff = CHESS_FILE( to ) - CHESS_FILE(from);
	int xdir = xdiff != 0 ? (xdiff / abs(xdiff)) : 0;

	int ydiff = CHESS_RANK( to ) - CHESS_RANK(from);
	int ydir = ydiff != 0 ? (ydiff / abs(ydiff)) : 0;

	int x = CHESS_FILE(from);
	int y = CHESS_RANK(from);

	x += xdir;
	y += ydir;

	while (((y * 8) + x) != to) {
		if (CW_NO_PIECE != board[((y * 8) + x)]) {
			return true;
		}

		x += xdir;
		y += ydir;
	}

	return false;
}

static void chess_perform_move( char* board, int from, int to )
{
	char piece = board[from];

	if (chess_is_en_passant_capture(board, piece, from, to)) {
		if ('p' == piece) {
			board[to + 8] = CW_NO_PIECE;
		} else {
			board[to - 8] = CW_NO_PIECE;
		}
	}

	int rookfrom = 0;
	int rookto = 0;
	if ( chess_is_castling( board, piece, from, to,
			&rookfrom, &rookto) ) {
		board[ rookto ] = board[ rookfrom ];
		board[ rookfrom ] = CW_NO_PIECE;
	}

	board[from] = CW_NO_PIECE;
	board[to] = piece;
}

#ifndef __chess_h__
#define __chess_h__

#include <stdbool.h>

/* check if move is possible, does not check full validity, you have to check */
/* for check also */
bool chess_is_possible_move(const char* board, int from, int to, char piece, bool capture);

/* check if a king is in check */
bool chess_is_in_check(const char* board, char kingpiece);

bool chess_is_mated(const char* board, char kingpiece);

bool chess_is_en_passant_capture(const char* board, char piece, int from, int to);

bool chess_is_castling(const char* board, char piece, int from, int to,
		int* rookfrom, int* rookto);

#endif /* __chess_h__ */

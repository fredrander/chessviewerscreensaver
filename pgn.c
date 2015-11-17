#include "pgn.h"
#include "pgnparser.h"
#include "log.h"
#include "chess.h"
#include "dbgutil.h"

#include <memory.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/**************************************************/

#define PGN_START_BOARD_FEN "rnbqkbnr/pppppppp/8/8/8/8/" \
                            "PPPPPPPP/RNBQKBNR w KQkq - 0 1"

GameInfo pgn_gameinfo;
Position pgn_gameposition;
Move pgn_move;
Color pgn_nextmovecolor = WHITE;

/**************************************************/

static void pgn_update_info(const char* tag, const char* value);
static void pgn_update_move_normal(int movenum, const char* movestr, char piece, int from, int to);
static void pgn_update_move_capture(int movenum, const char* movestr, char piece, int from, int to);
static void pgn_update_move_castle(int movenum, const char* movestr, char kingpiece, bool queenside);
static void pgn_update_move_en_passant(int movenum, const char* movestr, char pawnpiece, int from, int to, int enpassantcapturepos);
static void pgn_update_move_promote(int movenum, const char* movestr, char pawnpiece, int from, int to, char promotepiece);
static void pgn_next_color();
static char pgn_piece_for_color_to_move(char pgnpiece);
static int pgn_find_from_pos(Position* pos, int to, char piece, bool capture, int disambiguityfile, int disambiguityrank);
static void pgn_parse_move(int movenum, const char* pgn);
static void pgn_parse_result(const char* resultstr);
static void pgn_fen_to_position(const char* fen, Position* pos);
static bool pgn_get_color_from_fen(const char* fen, Color* color);
static char* pgn_game_info_alloc_and_save_str(char* ptr, const char* str);
static void pgn_game_info_save_str(char* ptr, const char* str, size_t maxlen);
static void pgn_free_game_info(GameInfo* info);
static void pgn_long_notation( int from, int to, char promotepiece, char* long_algebraic_str );
static void pgn_perform_move( char piece, int from, int to, char promotepiece, Position* pos );
static bool pgn_disambiguity_marker( char piece, int from, int to, Position* pos, char* marker );
static bool pgn_init_next_game();

/**************************************************/

bool pgn_init(const char* filename)
{
	/* my parser will handle the pgn file stuff */
	if (!pgn_parser_init(filename)) {
		return false;
	}

	/* clear game info */
	memset(&pgn_gameinfo, 0, sizeof(GameInfo));

	return true;
}

void pgn_close()
{
	pgn_parser_close();

	pgn_free_game_info(&pgn_gameinfo);
}

bool pgn_next_game()
{
	pgn_free_game_info(&pgn_gameinfo);

	pgn_parser_next_game();

	return pgn_init_next_game();
}

bool pgn_next_random_game()
{
	pgn_free_game_info(&pgn_gameinfo);

	pgn_parser_next_random_game();

	return pgn_init_next_game();
}

const Position* pgn_position()
{
	return &pgn_gameposition;
}

const Move* pgn_next_move()
{
	if (!pgn_parser_parse_move_list(pgn_parse_move, pgn_parse_result)) {
		return NULL;
	}
	return &pgn_move;
}

Color pgn_next_to_move()
{
	return pgn_nextmovecolor;
}

const GameInfo* pgn_game_info()
{
	return &pgn_gameinfo;
}

bool pgn_long_algebraic_to_pgn( char* long_algebraic, char* pgn, Position* pos )
{
	if ( long_algebraic == NULL || strlen(long_algebraic) < 4 ) {
		return false;
	}

	int from = (long_algebraic[ 0 ] - 'a') + ( (long_algebraic[ 1 ] - '1') * 8 );
	int to = (long_algebraic[ 2 ] - 'a') + ( (long_algebraic[ 3 ] - '1') * 8 );

	if ( pos == NULL ) {
		return false;
	}

	Position pos_copy;
	memcpy( &pos_copy, pos, sizeof(Position) );

	dbgutil_test( from < CW_NB_OF_SQUARES );
	dbgutil_test( to < CW_NB_OF_SQUARES );

	char frompiece = pos_copy.board[ from ];
	char topiece = pos_copy.board[ to ];
	char promotepiece = CW_NO_PIECE;
	if ( strlen(long_algebraic) > 4 ) {
		promotepiece = long_algebraic[ 4 ];
		if ( isupper( frompiece ) ) {
			promotepiece = toupper( promotepiece );
		} else {
			promotepiece = tolower( promotepiece );
		}
	}

	dbgutil_test( frompiece != CW_NO_PIECE );
	if ( frompiece == CW_NO_PIECE ) {
		return false;
	}

	char* wp = pgn;

	if ( (frompiece == 'k' || frompiece == 'K') &&
			abs( long_algebraic[ 2 ] - long_algebraic[ 0 ] ) > 1 ) {

		/* castle */
		if ( long_algebraic[ 2 ] == 'c' ) {
			strcpy( wp, "O-O-O" );
			wp += 5;
		} else {
			strcpy( wp, "O-O" );
			wp += 3;
		}

	} else if ( frompiece != 'p' && frompiece != 'P' ) {

		char disambiguity_marker[ 3 ];
		bool disambiguity = pgn_disambiguity_marker( frompiece, from, to,
				&pos_copy, disambiguity_marker );

		*wp = toupper(frompiece);
		wp++;

		if ( disambiguity ) {
			char* s = disambiguity_marker;
			while ( *s != '\0' ) {

				*wp = *s;
				wp++;
				s++;
			}
		}

		if ( topiece != CW_NO_PIECE ) {
			*wp = 'x';
			wp++;
		}
		*wp = long_algebraic[ 2 ];
		wp++;
		*wp = long_algebraic[ 3 ];
		wp++;
	} else {
		/* pawn */
		if ( long_algebraic[ 0 ] != long_algebraic[ 2 ] ) {
			*wp = long_algebraic[ 0 ];
			wp++;
			*wp = 'x';
			wp++;
		}
		*wp = long_algebraic[ 2 ];
		wp++;
		*wp = long_algebraic[ 3 ];
		wp++;
		if ( promotepiece != CW_NO_PIECE ) {
			*wp = '=';
			wp++;
			*wp = toupper( promotepiece );
			wp++;
		}
	}

	/* perform move */
	pgn_perform_move( frompiece, from, to, promotepiece, &pos_copy );

	bool white_check = chess_is_in_check( pos_copy.board, 'K' );
	bool black_check = chess_is_in_check( pos_copy.board, 'k' );
	if ( white_check || black_check ) {

		if ( (white_check && chess_is_mated( pos_copy.board, 'K' )) ||
			(black_check && chess_is_mated( pos_copy.board, 'k' )) ) {

			*wp = '#';
		} else {
			*wp = '+';
		}
		wp++;
	}

	*wp = '\0';
	wp++;

	return true;
}

bool pgn_long_algebraic_perform_move( char* long_algebraic, Position* pos )
{
	if ( long_algebraic == NULL || strlen(long_algebraic) < 4 ) {
		return false;
	}

	int from = (long_algebraic[ 0 ] - 'a') + ( (long_algebraic[ 1 ] - '1') * 8 );
	int to = (long_algebraic[ 2 ] - 'a') + ( (long_algebraic[ 3 ] - '1') * 8 );

	if ( pos == NULL ) {
		return false;
	}

	dbgutil_test( from < CW_NB_OF_SQUARES );
	dbgutil_test( to < CW_NB_OF_SQUARES );

	char frompiece = pos->board[ from ];
	char promotepiece = CW_NO_PIECE;
	if ( strlen(long_algebraic) > 4 ) {
		promotepiece = long_algebraic[ 4 ];
		if ( isupper( frompiece ) ) {
			promotepiece = toupper( promotepiece );
		} else {
			promotepiece = tolower( promotepiece );
		}
	}

	dbgutil_test( frompiece != CW_NO_PIECE );

	char piece = frompiece;
	if ( promotepiece != CW_NO_PIECE ) {
		piece = toupper( promotepiece );
	}

	pgn_perform_move( piece, from, to, promotepiece, pos );

	return true;
}

/**************************************************/

static void pgn_update_info(const char* tag, const char* value)
{
	if (strcmp(tag, "White") == 0) {
		pgn_gameinfo.white = pgn_game_info_alloc_and_save_str(pgn_gameinfo.white, value);
	} else if (strcmp(tag, "Black") == 0) {
		pgn_gameinfo.black = pgn_game_info_alloc_and_save_str(pgn_gameinfo.black, value);
	} else if (strcmp(tag, "Event") == 0) {
		pgn_gameinfo.event = pgn_game_info_alloc_and_save_str(pgn_gameinfo.event, value);
	} else if (strcmp(tag, "Site") == 0) {
		pgn_gameinfo.site = pgn_game_info_alloc_and_save_str(pgn_gameinfo.site, value);
	} else if (strcmp(tag, "Round") == 0 && strcmp(value, "?") != 0) {
		pgn_gameinfo.round = pgn_game_info_alloc_and_save_str(pgn_gameinfo.round, value);
	} else if (strcmp(tag, "FEN") == 0) {
		pgn_gameinfo.fen = pgn_game_info_alloc_and_save_str(pgn_gameinfo.fen, value);
	} else if (strcmp(tag, "Date") == 0) {
		pgn_game_info_save_str(pgn_gameinfo.datestr, value, PGN_MAX_LEN_DATE);
	} else if (strcmp(tag, "ECO") == 0) {
		pgn_game_info_save_str(pgn_gameinfo.eco, value, PGN_MAX_LEN_ECO);
	} else if (strcmp(tag, "WhiteElo") == 0) {
		pgn_game_info_save_str(pgn_gameinfo.whiteelo, value, PGN_MAX_LEN_ELO);
	} else if (strcmp(tag, "BlackElo") == 0) {
		pgn_game_info_save_str(pgn_gameinfo.blackelo, value, PGN_MAX_LEN_ELO);
	}
}

static char pgn_piece_for_color_to_move(char pgnpiece)
{
	/* white uppercase, black lowercase */
	if (WHITE == pgn_nextmovecolor) {
		return toupper(pgnpiece);
	} else {
		return tolower(pgnpiece);
	}
}

static void pgn_update_move_normal(int movenum, const char* movestr, char piece, int from, int to)
{
	memset(&pgn_move, 0, sizeof(Move));

	pgn_move.type = NORMAL;
	pgn_move.movenum = movenum;
	strncpy(pgn_move.movestr, movestr, CW_MAX_MOVE_STRING - 1);
	pgn_move.piece = piece;
	pgn_move.from = from;
	pgn_move.to = to;

	pgn_long_notation( pgn_move.from, pgn_move.to, CW_NO_PIECE, pgn_move.long_algebraic );

	LOG(INFO, "Next move: %c %d  -->  %d", piece, from, to);

	pgn_gameposition.board[pgn_move.from] = CW_NO_PIECE;
	pgn_gameposition.board[pgn_move.to] = pgn_move.piece;
}

static void pgn_update_move_capture(int movenum, const char* movestr, char piece, int from, int to)
{
	memset(&pgn_move, 0, sizeof(Move));

	pgn_move.type = CAPTURE;
	pgn_move.movenum = movenum;
	strncpy(pgn_move.movestr, movestr, CW_MAX_MOVE_STRING - 1);
	pgn_move.piece = piece;
	pgn_move.from = from;
	pgn_move.to = to;
	pgn_move.capturepiece = pgn_gameposition.board[pgn_move.to];

	pgn_long_notation( pgn_move.from, pgn_move.to, CW_NO_PIECE, pgn_move.long_algebraic );

	LOG(INFO, "Next move: %c %d  x  %d", piece, from, to);

	pgn_gameposition.board[pgn_move.from] = CW_NO_PIECE;
	pgn_gameposition.board[pgn_move.to] = pgn_move.piece;
}

static void pgn_update_move_castle(int movenum, const char* movestr, char kingpiece, bool queenside)
{
	memset(&pgn_move, 0, sizeof(Move));

	pgn_move.type = CASTLE;
	pgn_move.movenum = movenum;
	strncpy(pgn_move.movestr, movestr, CW_MAX_MOVE_STRING - 1);

	pgn_move.piece = kingpiece;

	if ('K' == kingpiece) {
		pgn_move.castlerookpiece = 'R';
	} else {
		pgn_move.castlerookpiece = 'r';
	}

	if (queenside) {
		/* queen side castle */

		if ('K' == kingpiece) {
			pgn_move.from = 4;
			pgn_move.to = 2;

			pgn_move.castlerookfrom = 0;
			pgn_move.castlerookto = 3;
		} else {
			pgn_move.from = 60;
			pgn_move.to = 58;

			pgn_move.castlerookfrom = 56;
			pgn_move.castlerookto = 59;
		}
	} else {
		/* king side castle */
		if ('K' == kingpiece) {
			pgn_move.from = 4;
			pgn_move.to = 6;

			pgn_move.castlerookfrom = 7;
			pgn_move.castlerookto = 5;
		} else {
			pgn_move.from = 60;
			pgn_move.to = 62;

			pgn_move.castlerookfrom = 63;
			pgn_move.castlerookto = 61;
		}
	}

	pgn_long_notation( pgn_move.from, pgn_move.to, CW_NO_PIECE, pgn_move.long_algebraic );

	LOG(INFO, "Next move: Castle %c %d  -->  %d", pgn_move.piece, pgn_move.from, pgn_move.to);

	pgn_gameposition.board[pgn_move.from] = CW_NO_PIECE;
	pgn_gameposition.board[pgn_move.to] = pgn_move.piece;
	pgn_gameposition.board[pgn_move.castlerookfrom] = CW_NO_PIECE;
	pgn_gameposition.board[pgn_move.castlerookto] = pgn_move.castlerookpiece;
}

static void pgn_update_move_en_passant(int movenum, const char* movestr, char pawnpiece, int from, int to, int enpassantcapturepos)
{
	memset(&pgn_move, 0, sizeof(Move));

	pgn_move.type = EN_PASSANT;
	pgn_move.movenum = movenum;
	strncpy(pgn_move.movestr, movestr, CW_MAX_MOVE_STRING - 1);

	pgn_move.piece = pawnpiece;
	pgn_move.from = from;
	pgn_move.to = to;
	pgn_move.enpassantcapturepos = enpassantcapturepos;

	pgn_long_notation( pgn_move.from, pgn_move.to, CW_NO_PIECE, pgn_move.long_algebraic );

	LOG(INFO, "Next move: %c %d  -->  %d  En passant", pgn_move.piece, pgn_move.from, pgn_move.to);

	pgn_gameposition.board[pgn_move.from] = CW_NO_PIECE;
	pgn_gameposition.board[pgn_move.to] = pgn_move.piece;
	pgn_gameposition.board[pgn_move.enpassantcapturepos] = CW_NO_PIECE;
}

static void pgn_update_move_promote(int movenum, const char* movestr, char pawnpiece, int from, int to, char promotepiece)
{
	memset(&pgn_move, 0, sizeof(Move));

	pgn_move.type = PROMOTE;
	pgn_move.movenum = movenum;
	strncpy(pgn_move.movestr, movestr, CW_MAX_MOVE_STRING - 1);

	pgn_move.piece = pawnpiece;
	pgn_move.from = from;
	pgn_move.to = to;
	pgn_move.promotepiece = promotepiece;
	pgn_move.capturepiece = pgn_gameposition.board[pgn_move.to];

	pgn_long_notation( pgn_move.from, pgn_move.to, pgn_move.promotepiece, pgn_move.long_algebraic );

	LOG(INFO, "Next move: %c %d  -->  %d  =  %c", pgn_move.piece, pgn_move.from, pgn_move.to, pgn_move.promotepiece);

	pgn_gameposition.board[pgn_move.from] = CW_NO_PIECE;
	pgn_gameposition.board[pgn_move.to] = pgn_move.promotepiece;
}

static int pgn_find_from_pos(Position* pos, int to, char piece, bool capture, int disambiguityfile, int disambiguityrank)
{
	int startfile = 0;
	int endfile = 7;
	if (0 <= disambiguityfile) {
		startfile = disambiguityfile;
		endfile = disambiguityfile;
	}

	int startrank = 0;
	int endrank = 7;
	if (0 <= disambiguityrank) {
		startrank = disambiguityrank;
		endrank = disambiguityrank;
	}

	for (int r = startrank; r <= endrank; ++r) {
		for (int f = startfile; f <= endfile; ++f) {
			int from = (CW_NB_OF_FILES * r) + f;
			char boardpiece = pos->board[from];
			if (piece == boardpiece) {
				if (chess_is_possible_move(pos->board, from, to, piece, capture)) {
					/* the move is not possible if under check */
					/* we have to perform the move */
					Position tmppos;
					memcpy(&tmppos, pos, sizeof(Position));
					pgn_perform_move( piece, from, to, CW_NO_PIECE, &tmppos );

					char kingpiece = isupper(piece) ? 'K' : 'k';
					if (!chess_is_in_check(tmppos.board, kingpiece)) {
						return from;
					}
				}
			}
		}
	}

	/* no success */
	return -1;
}

static void pgn_parse_move(int movenum, const char* pgn)
{
	dbgutil_test(NULL != pgn);

	LOG(INFO, "PGN move %d: %s", movenum, pgn);

	size_t len = strlen(pgn);

	if (2 > len) {
		/* nothing to work with */
		return;
	}

	int captureidx = -1;
	int castlecnt = 0;
	int promoteidx = -1;
	int destinationidx = len - 2;
	int disambiguityendidx = len - 3;

	/* Qa6xb7# */

	/* let's walk through pgn move and gather some basic info */
	/* save index to markers for destination, capture etc. */
	for (int i = 0; i < len; ++i) {
		switch (pgn[i]) {
		case 'O':
			castlecnt++;
			break;

		case 'x':
			captureidx = i;
			destinationidx = i + 1;
			disambiguityendidx = i - 1;
			break;

		case '=':
			promoteidx = i;
			destinationidx = i - 2;
			if (0 > captureidx) {
				disambiguityendidx = i - 3; /* impossible since only pawns promote */
			}
			break;

		case '+':
			case '#':
			if (0 > captureidx && 0 > promoteidx) {
				/* no capture or promote, then destination and disambiguity is ahead of check/mate marker */
				destinationidx = i - 2;
				disambiguityendidx = i - 3;
			}
			break;
		}
	}

	/* get rid of special case; castle */
	if (1 < castlecnt) {
		pgn_update_move_castle(movenum, pgn, pgn_piece_for_color_to_move('K'), 2 < castlecnt);
		pgn_next_color();
		return;
	}

	int disambiguitystartidx = -1;

	/* what piece? */
	char piece = CW_NO_PIECE;
	if (isupper(pgn[0])) {
		/* piece move */
		piece = pgn_piece_for_color_to_move(pgn[0]);
		disambiguitystartidx = 1;
	} else {
		/* pawn move */
		piece = pgn_piece_for_color_to_move('P');
		disambiguitystartidx = 0;
	}

	/* get destination pos */
	if (destinationidx + 2 > len) {
		LOG(ERROR, "Failed to get destination from index %d (length: %d)", destinationidx, len);
		return;
	}

	int to = (pgn[destinationidx] - 'a') + (8 * (pgn[destinationidx + 1] - '1'));

	if (to < 0 || to > 63) {
		LOG(ERROR, "Invalid destination square %d", to);
		return;
	}

	/* get destination piece */
	char promotepiece = CW_NO_PIECE;
	if (0 <= promoteidx && (promoteidx + 1) < len) {
		promotepiece = pgn_piece_for_color_to_move(pgn[promoteidx + 1]);
	}

	/* get disambiguity rank and/or file */
	int disambiguityrank = -1;
	int disambiguityfile = -1;
	while (disambiguitystartidx >= 0 && disambiguitystartidx <= disambiguityendidx) {
		if (isdigit(pgn[disambiguitystartidx])) {
			disambiguityrank = pgn[disambiguitystartidx] - '1';
		} else {
			disambiguityfile = pgn[disambiguitystartidx] - 'a';
		}
		++disambiguitystartidx;
	}

	/* search for from pos */
	int from = pgn_find_from_pos(&pgn_gameposition, to, piece, 0 <= captureidx, disambiguityfile, disambiguityrank);

	if (0 > from) {
		LOG(ERROR, "Failed to find from position for move %s", pgn);
		return;
	}

	if (CW_NO_PIECE != promotepiece) {
		pgn_update_move_promote(movenum, pgn, piece, from, to, promotepiece);
	} else if (chess_is_en_passant_capture(pgn_gameposition.board, piece, from, to)) {
		if ('P' == piece) {
			pgn_update_move_en_passant(movenum, pgn, piece, from, to, to - 8);
		} else {
			pgn_update_move_en_passant(movenum, pgn, piece, from, to, to + 8);
		}
	} else if (0 <= captureidx) {
		pgn_update_move_capture(movenum, pgn, piece, from, to);
	} else {
		pgn_update_move_normal(movenum, pgn, piece, from, to);
	}
	pgn_next_color();
}

static void pgn_parse_result(const char* resultstr)
{
	dbgutil_test(NULL != resultstr);

	LOG(INFO, "Game ended with result: %s", resultstr);

	if ( strcmp( resultstr, "1-0" ) == 0 ) {
		pgn_gameinfo.result = WHITE_WIN;
	} else if ( strcmp( resultstr, "0-1" ) == 0 ) {
		pgn_gameinfo.result = BLACK_WIN;
	} else if ( strcmp( resultstr, "1/2-1/2" ) == 0 ) {
		pgn_gameinfo.result = DRAW;
	} else {
		pgn_gameinfo.result = UNKNOWN;
	}
}

static void pgn_next_color()
{
	if (WHITE == pgn_nextmovecolor) {
		pgn_nextmovecolor = BLACK;
	} else {
		pgn_nextmovecolor = WHITE;
	}
}

static void pgn_fen_to_position(const char* fen, Position* p)
{
	dbgutil_test(NULL != fen);
	dbgutil_test(NULL != p);

	memset(p, 0, sizeof(Position));

	/* FEN starts from upper left of board (at a8, that is) */
	int rank = 7;
	int file = 0;
	while (*fen && ' ' != *fen) {
		if ('1' <= *fen && '8' >= *fen) {
			file += *fen - '0';
		} else if ('/' == *fen) {
			--rank;
			file = 0;
		} else {
			int boardpos = (CW_NB_OF_FILES * rank) + file;
			dbgutil_test(CW_NB_OF_SQUARES > boardpos);
			p->board[boardpos] = *fen;

			++file;
		}

		++fen;
	}
}

static bool pgn_get_color_from_fen(const char* fen, Color* color)
{
	dbgutil_test(NULL != fen);
	dbgutil_test(NULL != color);

	/* color to move after first space */
	char* cpos = strchr(fen, ' ');
	if ( NULL == cpos) {
		return false;
	}

	++cpos;

	if ('w' == *cpos) {
		*color = WHITE;
	} else if ('b' == *cpos) {
		*color = BLACK;
	} else {
		return false;
	}

	return true;
}

static char* pgn_game_info_alloc_and_save_str(char* ptr, const char* str)
{
	char* result = NULL;
	size_t size = sizeof(char) * (strlen(str) + 1);

	result = realloc(ptr, size);

	if ( NULL != result) {
		strncpy(result, str, size);
	}

	return result;
}

static void pgn_game_info_save_str(char* ptr, const char* str, size_t maxlen)
{
	strncpy(ptr, str, maxlen);
	ptr[maxlen] = '\0';
}

#define PGN_SAFE_FREE( p ) if ( NULL != p ) { free( p ); p = NULL; }

static void pgn_free_game_info(GameInfo* info)
{
	PGN_SAFE_FREE(info->white);
	PGN_SAFE_FREE(info->black);
	PGN_SAFE_FREE(info->event);
	PGN_SAFE_FREE(info->round);
	PGN_SAFE_FREE(info->site);
	PGN_SAFE_FREE(info->fen);

	/* clear all */
	memset(info, 0, sizeof(GameInfo));
}

static void pgn_long_notation( int from, int to, char promotepiece, char* long_algebraic_str )
{
	long_algebraic_str[ 0 ] = 'a' + (from & 7);
	long_algebraic_str[ 1 ] = '1' + (from >> 3);
	long_algebraic_str[ 2 ] = 'a' + (to & 7);
	long_algebraic_str[ 3 ] = '1' + (to >> 3);
	if ( promotepiece != CW_NO_PIECE ) {
		long_algebraic_str[ 4 ] = promotepiece;
		long_algebraic_str[ 5 ] = '\0';
	} else {
		long_algebraic_str[ 4 ] = '\0';
	}
}

static void pgn_perform_move( char piece, int from, int to,
		char promotepiece, Position* pos )
{
	if (chess_is_en_passant_capture(pos->board, piece, from, to)) {
		if ('p' == piece) {
			pos->board[to + 8] = CW_NO_PIECE;
		} else {
			pos->board[to - 8] = CW_NO_PIECE;
		}
	}

	int rookfrom = 0;
	int rookto = 0;
	if ( chess_is_castling( pos->board, piece, from, to,
			&rookfrom, &rookto) ) {
		pos->board[ rookto ] = pos->board[ rookfrom ];
		pos->board[ rookfrom ] = CW_NO_PIECE;
	}

	pos->board[from] = CW_NO_PIECE;
	if ( promotepiece != CW_NO_PIECE ) {
		pos->board[to] = promotepiece;
	} else {
		pos->board[to] = piece;
	}
}

static bool pgn_disambiguity_marker( char piece, int from, int to, Position* pos, char* marker )
{
	if ( tolower( piece ) == 'k' || tolower( piece ) == 'p' ) {
		return false;
	}

	bool capture = pos->board[ to ] != CW_NO_PIECE;
	char my_king = isupper( piece ) ? 'K' : 'k';

	int possible_sources[ CW_NB_OF_SQUARES ];
	int cnt = 0;

	for ( int i = 0; i < CW_NB_OF_SQUARES; ++i ) {

		if ( pos->board[ i ] == piece ) {

			if ( chess_is_possible_move( pos->board, i, to, piece, capture ) ) {

				Position pos_copy;
				memcpy( &pos_copy, pos, sizeof(Position) );

				pgn_perform_move( piece, i, to, CW_NO_PIECE, &pos_copy );

				if ( !chess_is_in_check( pos_copy.board, my_king ) ) {
					/* possible */
					possible_sources[ cnt ] = i;
					cnt++;
				}
			}
		}
	}

	bool conflict_file = false;
	bool conflict_rank = false;
	for ( int i = 0; i < cnt; ++i ) {
		int f = (possible_sources[ i ] & 7);
		int r = (possible_sources[ i ] >> 3);
		for ( int j = i + 1; j < cnt; ++j ) {
			int fn = (possible_sources[ j ] & 7);
			if ( f == fn ) {
				conflict_file = true;
			}
			int rn = (possible_sources[ j ] >> 3);
			if ( r == rn ) {
				conflict_rank = true;
			}
		}
	}

	if ( cnt > 1 ) {
		if ( conflict_file && conflict_rank ) {
			sprintf( marker, "%c%c", 'a' + ( from & 7 ), '1' + ( from >> 3 ) );
		} else if ( conflict_file ) {
			sprintf( marker, "%c", '1' + ( from >> 3 ) );
		} else {
			sprintf( marker, "%c", 'a' + ( from & 7 ) );
		}
	}

	return (cnt > 1);
}

static bool pgn_init_next_game()
{
	if (!pgn_parser_parse_info(pgn_update_info)) {
		return false;
	}

	const char* fen = pgn_gameinfo.fen;
	if ( NULL == fen) {
		/* no FEN in game, use new board */
		fen = PGN_START_BOARD_FEN;
	}

	pgn_fen_to_position(fen, &pgn_gameposition);
	pgn_get_color_from_fen(fen, &pgn_nextmovecolor);

	return true;
}

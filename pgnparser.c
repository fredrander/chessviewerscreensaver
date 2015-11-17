#include "pgnparser.h"
#include "defs.h"
#include "log.h"
#include "pgnbuiltin.h"
#include "dbgutil.h"

#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/****************************************************/

/* game info state */
typedef enum
{
	GAME_START,
	NEXT_TAG,
	TAG_START,
	TAG,
	VALUE_START,
	VALUE
} GameInfoState;

typedef enum
{
	MOVE_START,
	MOVE,
	MOVE_NUM,
	NAG,
	COMMENT,
	COMMENT_EOL,
	VARIANT,
	ESCAPE
} MoveListState;

/****************************************************/

GameInfoState pgnparser_infostate = GAME_START;
MoveListState pgnparser_moveliststate = MOVE_START;

#define READ_BUF_SIZE 8096

#define TAG_MAX_LEN 63
#define VALUE_MAX_LEN 255

FILE* pgnparser_file = NULL;
char pgnparser_rdbuf[READ_BUF_SIZE];
size_t pgnparser_bufcnt = 0;
size_t pgnparser_readpos = 0;
size_t pgnparser_fposlastread = 0;
size_t pgnparser_fposgame = 0;

#define PGN_PARSER_FILE_NAME_STATE "/tmp/.chessviewerscreensaver"

/****************************************************/

#define PGN_PARSER_IS_MOVE_CHAR( ch ) ( isalnum( ch ) || '-' == ch || '=' == ch || '+' == ch || '#' == ch )
#define PGN_PARSER_CLEAR_STR( str, strpos ) { strpos = 0; str[ strpos ] = '\0'; }
#define PGN_PARSER_ADD_CHAR_TO_STR( ch, str, strpos ) { str[ strpos ] = ch; str[ strpos + 1 ] = '\0'; strpos++; }

/****************************************************/

static char pgn_parser_read_char();
static size_t pgn_parser_fpos();
static bool pgn_parser_save_state( const char* fname );
static bool pgn_parser_load_state( const char* fname );

/****************************************************/

bool pgn_parser_init(const char* filename)
{
	srand( time(NULL) );

	if ( filename != NULL ) {
		pgnparser_file = fopen(filename, "r");

		if ( NULL == pgnparser_file) {
			return false;
		}

		/* go to start position of last game */
		if ( pgn_parser_load_state( PGN_PARSER_FILE_NAME_STATE ) ) {
			if ( 0 == fseek( pgnparser_file, pgnparser_fposgame, SEEK_SET ) ) {
				pgnparser_fposlastread = pgnparser_fposgame;
			}
		}
	}

	return true;
}

void pgn_parser_close()
{
	if ( NULL != pgnparser_file ) {
		fclose(pgnparser_file);
		pgnparser_file = NULL;
	}
}

void pgn_parser_next_game()
{
	pgnparser_infostate = GAME_START;
	pgnparser_moveliststate = MOVE_START;

	/* save file pos. for start of game */
	pgnparser_fposgame = pgn_parser_fpos();

	pgn_parser_save_state( PGN_PARSER_FILE_NAME_STATE );
}

void pgn_parser_next_random_game()
{
	pgnparser_infostate = GAME_START;
	pgnparser_moveliststate = MOVE_START;

	if ( pgnparser_file == NULL ) {
		return;
	}

	fseek( pgnparser_file, 0, SEEK_END );
	int64_t fsize = ftell( pgnparser_file );

	int64_t rval =
		(((int64_t) rand() <<  0) & 0x000000000000FFFFull) |
		(((int64_t) rand() << 16) & 0x00000000FFFF0000ull) |
		(((int64_t) rand() << 32) & 0x0000FFFF00000000ull) |
		(((int64_t) rand() << 48) & 0xFFFF000000000000ull);

	int64_t fpos = rval % fsize;

	fseek( pgnparser_file, (long int) labs( fpos ), SEEK_SET );

	pgnparser_bufcnt = 0;

	bool found_start = false;
	bool newline = false;
	while ( !found_start ) {
		char c = pgn_parser_read_char();

		found_start = ( newline && c != '[' );
		newline = ( c == '\n');
	}
	bool found = false;
	newline = false;
	while ( !found ) {
		char c = pgn_parser_read_char();

		found = ( newline && c == '[' );
		newline = ( c == '\n');
	}
	pgnparser_infostate = TAG; /* since we've already read [ */

	LOG( INFO, "Game start pos: %u", pgn_parser_fpos() );
}

bool pgn_parser_parse_info(PgnParserGameInfoCallback callback)
{
	char tag[TAG_MAX_LEN + 1];
	char value[VALUE_MAX_LEN + 1];
	size_t strpos = 0;
	char ch = '\0';

	bool done = false;

	while (!done) {
		ch = pgn_parser_read_char();
		done = ('\0' == ch);

		if (!done) {

			switch (pgnparser_infostate) {
			case GAME_START:
				/* at game start go to first [ */
				if ('[' == ch) {
					PGN_PARSER_CLEAR_STR(tag, strpos); /* clear tag */
					pgnparser_infostate = TAG;
				}
				break;

			case TAG_START:
				if ('[' == ch) {
					PGN_PARSER_CLEAR_STR(tag, strpos); /* clear tag */
					pgnparser_infostate = TAG;
				}
				if ('\n' == ch) {
					/* no more tags */
					pgnparser_infostate = TAG_START;
					done = true;
				}
				break;

			case TAG:
				if (' ' == ch) {
					pgnparser_infostate = VALUE_START;
				} else if ( TAG_MAX_LEN > strpos) {
					PGN_PARSER_ADD_CHAR_TO_STR(ch, tag, strpos);
				}
				break;

			case VALUE_START:
				if ('"' == ch) {
					PGN_PARSER_CLEAR_STR(value, strpos); /* clear value */
					pgnparser_infostate = VALUE;
				}
				break;

			case VALUE:
				if ('"' == ch) {
					/* tag - value done, report */
					pgnparser_infostate = NEXT_TAG;
					if ( NULL != callback) {
						callback(tag, value);
					}
				} else if ( VALUE_MAX_LEN > strpos) {
					PGN_PARSER_ADD_CHAR_TO_STR(ch, value, strpos);
				}
				break;

			case NEXT_TAG:
				/* goto new line */
				if ('\n' == ch) {
					pgnparser_infostate = TAG_START;
				}
				break;
			}
		}
	}

	return true;
}

bool pgn_parser_parse_move_list(PgnParserMoveCallback callbackmove,
		PgnParserGameResultCallback callbackresult)
{
	char movestr[CW_MAX_MOVE_STRING];
	size_t strpos = 0;
	int movenum = -1;
	char ch = '\0';

	int variantcnt = 0;

	bool done = false;

	dbgutil_test( NULL != callbackmove );
	dbgutil_test( NULL != callbackresult );

	while (!done) {
		ch = pgn_parser_read_char();
		done = ('\0' == ch);

		if (!done) {
			switch (pgnparser_moveliststate) {
			case MOVE_START:
				/* at move start we can have a digit for move number (not req.) or a move */
				/* at game end we have result or * */
				if (isdigit(ch)) {
					pgnparser_moveliststate = MOVE_NUM;
					movenum = ch - '0';
				} else if (PGN_PARSER_IS_MOVE_CHAR(ch)) {
					pgnparser_moveliststate = MOVE;
					PGN_PARSER_CLEAR_STR(movestr, strpos);
					PGN_PARSER_ADD_CHAR_TO_STR(ch, movestr, strpos);
				} else if ('*' == ch) {
					/* end of game */
					callbackresult("*");
					return false;
				} else if ('$' == ch) {
					pgnparser_moveliststate = NAG;
				} else if ('(' == ch) {
					variantcnt = 1;
					pgnparser_moveliststate = VARIANT;
				} else if ('{' == ch) {
					pgnparser_moveliststate = COMMENT;
				} else if (';' == ch) {
					pgnparser_moveliststate = COMMENT_EOL;
				} else if ('%' == ch) {
					/* escape, ignore line of text */
					/* TODO: should only trigger escape if % at first column of row */
					pgnparser_moveliststate = ESCAPE;
				}
				break;

			case MOVE_NUM:
				if (isdigit(ch)) {
					movenum = (movenum * 10) + (ch - '0');
				} else if ('.' == ch) {
					/* we might have three periods for black move number, but ignore those */
					pgnparser_moveliststate = MOVE_START;
				} else if (isspace(ch)) {
					pgnparser_moveliststate = MOVE_START;
				} else if ('-' == ch || '/' == ch) {
					/* this is not a move number but a result (1/2-1/2, 1-0 or 0-1) */
					if ( '/' == ch ) {
						callbackresult( "1/2-1/2" );
					} else if ( movenum == 1 ) {
						callbackresult( "1-0" );
					} else {
						callbackresult( "0-1" );
					}
					return false;
				}
				break;

			case MOVE:

				if (PGN_PARSER_IS_MOVE_CHAR(ch)) {
					if ( (CW_MAX_MOVE_STRING - 1) > strpos) {
						PGN_PARSER_ADD_CHAR_TO_STR(ch, movestr, strpos);
					}
				} else {
					/* move done */
					pgnparser_moveliststate = MOVE_START;

					callbackmove(movenum, movestr);
					return true;
				}

				break;

			case NAG:
				if (!isdigit(ch)) {
					pgnparser_moveliststate = MOVE_START;
				}
				break;

			case COMMENT:
				if ('}' == ch) {
					pgnparser_moveliststate = MOVE_START;
				}
				break;

			case COMMENT_EOL:
				if ('\n' == ch) {
					pgnparser_moveliststate = MOVE_START;
				}
				break;

			case VARIANT:
				if (')' == ch) {
					variantcnt--;
					if (0 == variantcnt) {
						pgnparser_moveliststate = MOVE_START;
					}
				} else if ('(' == ch) {
					/* variant can be nested */
					variantcnt++;
				}
				break;

			case ESCAPE:
				if ('\n' == ch) {
					pgnparser_moveliststate = MOVE_START;
				}
				break;
			}
		}
	}

	return true;
}

/****************************************************/

static char pgn_parser_read_char()
{
	if ( pgnparser_file == NULL ) {
		return pgnbuiltin_get();
	}

	if (pgnparser_readpos >= pgnparser_bufcnt) {
		/* read more data */
		dbgutil_test(NULL != pgnparser_file);

		pgnparser_fposlastread = ftell( pgnparser_file );
		pgnparser_bufcnt = fread(pgnparser_rdbuf, sizeof(char),
				READ_BUF_SIZE, pgnparser_file);
		if ( pgnparser_bufcnt < 1 ) {
			fseek( pgnparser_file, 0, SEEK_SET );
			pgnparser_fposlastread = ftell( pgnparser_file );
			pgnparser_bufcnt = fread(pgnparser_rdbuf, sizeof(char),
					READ_BUF_SIZE, pgnparser_file);

		}
		pgnparser_readpos = 0;
	}

	if (pgnparser_readpos >= pgnparser_bufcnt) {
		/* NOK */
		return '\0';
	}

	char ch = pgnparser_rdbuf[pgnparser_readpos];
	pgnparser_readpos++;

	return ch;
}

static size_t pgn_parser_fpos()
{
	return pgnparser_fposlastread + pgnparser_readpos;
}

static bool pgn_parser_save_state( const char* fname )
{
	FILE* fp = fopen( fname, "w" );
	if ( NULL != fp ) {
		fwrite( &pgnparser_fposgame, sizeof(pgnparser_fposgame), 1, fp );
		fclose( fp );
		fp = NULL;

		LOG(INFO, "Saved game position: %u", pgnparser_fposgame);

		return true;
	}

	return false;
}

static bool pgn_parser_load_state( const char* fname )
{
	FILE* fp = fopen( fname, "r" );
	if ( NULL != fp ) {
		fread( &pgnparser_fposgame, sizeof(pgnparser_fposgame), 1, fp );
		fclose( fp );
		fp = NULL;

		LOG(INFO, "Loaded game position: %u", pgnparser_fposgame);

		return true;
	}

	return false;
}

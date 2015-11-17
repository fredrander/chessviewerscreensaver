#ifndef __pgnparser_h__
#define __pgnparser_h__

#include <stdbool.h>

bool pgn_parser_init(const char* filename);
void pgn_parser_close();

void pgn_parser_next_game();
void pgn_parser_next_random_game();

typedef void (*PgnParserGameInfoCallback)(const char* tag, const char* value);

bool pgn_parser_parse_info(PgnParserGameInfoCallback callback);

typedef void (*PgnParserMoveCallback)(int movenum, const char* movestr);
typedef void (*PgnParserGameResultCallback)(const char* resultstr);

bool pgn_parser_parse_move_list(PgnParserMoveCallback callbackmove,
		PgnParserGameResultCallback callbackresult);

#endif /* __pgnparser_h__ */

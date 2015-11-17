#ifndef __ui_h__
#define __ui_h__

#include <stdbool.h>

bool ui_init();
void ui_close();

void ui_flush();

void ui_clear();

void ui_toggle_board_position();

void ui_draw_game_info(const char* white,
		const char* black,
		const char* whiteelo,
		const char* blackelo,
		const char* event,
		const char* round,
		const char* site,
		const char* date,
		const char* eco,
		const char* ecoinfo);

void ui_draw_board();
void ui_draw_piece(char piece, int pos);

void ui_highlight_move(int from, int to);

void ui_draw_move_str(int movenum, bool white, const char* pgnstr);

void ui_draw_engine_eval( const char* scorestr, const char* evalstr );

void ui_draw_result( const char* resultstr );


#endif /* __ui_h__ */

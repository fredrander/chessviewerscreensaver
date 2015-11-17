#ifndef __movelist_h__
#define __movelist_h__


#include <stdbool.h>


void movelist_init();

void movelist_clear();

void movelist_add_half_move(int movenum, const char* movestr, bool white);

int movelist_size();

const char* movelist_get_white(int index);
const char* movelist_get_black(int index);
int movelist_get_move_num(int index);


#endif /* __movelist_h__ */

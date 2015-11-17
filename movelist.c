#include "movelist.h"
#include "defs.h"
#include "dbgutil.h"

#include <string.h>


/****************************************************************/

#define MOVELIST_MAX_MOVES 40

typedef struct
{
	int movenum;
	char whitemove[CW_MAX_MOVE_STRING];
	char blackmove[CW_MAX_MOVE_STRING];
} FullMove;

FullMove movelist[MOVELIST_MAX_MOVES];
int movelist_count = 0;
int movelist_end = 0;

/****************************************************************/

static int movelist_inc_and_wrap_around(int value, int inc);
static int movelist_get_pos_for_index(int index);

/****************************************************************/

void movelist_init()
{
	memset(movelist, 0, sizeof(movelist));
	movelist_count = 0;
	movelist_end = 0;
}

void movelist_clear()
{
	movelist_count = 0;
	movelist_end = 0;
}

int movelist_size()
{
	return movelist_count;
}

const char* movelist_get_white(int index)
{
	int pos = movelist_get_pos_for_index(index);
	return movelist[pos].whitemove;
}

const char* movelist_get_black(int index)
{
	int pos = movelist_get_pos_for_index(index);
	return movelist[pos].blackmove;
}

int movelist_get_move_num(int index)
{
	int pos = movelist_get_pos_for_index(index);
	return movelist[pos].movenum;
}

void movelist_add_half_move(int movenum, const char* movestr, bool white)
{
	dbgutil_test(strlen(movestr) < CW_MAX_MOVE_STRING);

	int pos = movelist_end;

	if (white) {
		movelist_count++;
		if (movelist_count > MOVELIST_MAX_MOVES) {
			movelist_count = MOVELIST_MAX_MOVES;
		}
	} else {

		movelist_end = movelist_inc_and_wrap_around(movelist_end, 1);
	}

	if (white) {
		movelist[pos].movenum = movenum;

		strncpy(movelist[pos].whitemove, movestr, CW_MAX_MOVE_STRING);
		movelist[pos].blackmove[0] = '\0';
	} else {
		strncpy(movelist[pos].blackmove, movestr, CW_MAX_MOVE_STRING);
	}
}

/*********************************************************************/

static int movelist_inc_and_wrap_around(int value, int inc)
{
	value += inc;
	value = value % MOVELIST_MAX_MOVES;

	dbgutil_test(value >= 0);
	dbgutil_test(value < MOVELIST_MAX_MOVES);

	return value;
}

static int movelist_get_pos_for_index(int index)
{
	dbgutil_test(index < movelist_count);

	int pos = index;
	if (movelist_count >= MOVELIST_MAX_MOVES) {
		if (movelist[movelist_end].blackmove[0] == '\0') {
			/* editing */
			pos = movelist_inc_and_wrap_around(movelist_end + 1, index);
		} else {
			pos = movelist_inc_and_wrap_around(movelist_end, index);
		}
	}

	return pos;
}

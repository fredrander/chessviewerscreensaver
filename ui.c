#include "ui.h"
#include "vroot.h"
#include "movelist.h"
#include "log.h"
#include "dbgutil.h"


#ifndef __USE_BSD
#define __USE_BSD /* for usleep */
#endif /* __USE_BSD */

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

/*******************************************************/

#define UI_COL_WHITE_SQUARE 0xaaaadd /*0xa6bce7*/ /*0xc8c365*/
#define UI_COL_BLACK_SQUARE 0x666688 /*0x5a6aad*/ /*0x77a26d*/

#define UI_COL_HIGHLIGHT_SQUARE 0xe5e500

#define UI_COL_BACKGROUND 0x000000

#define UI_SIZE_SQUARE_WIDTH (UI_SIZE_SQUARE_HEIGHT)
#define UI_SIZE_SQUARE_HEIGHT ((ui_screenheight * 10) / 90)

#define UI_SIZE_HIGHLIGHT_LINE_WIDTH 8

#define UI_SIZE_INFO_WIDTH ( ui_screenwidth - 8 * (UI_SIZE_SQUARE_WIDTH) - \
		(2 * UI_SPACING_X_INFO) - (UI_POS_BOARD_X_LEFT) )

#define UI_POS_BOARD_X_LEFT UI_POS_BOARD_Y
#define UI_POS_BOARD_X_RIGHT (ui_screenwidth - UI_POS_BOARD_X_LEFT - 8 * (UI_SIZE_SQUARE_WIDTH) )
#define UI_POS_BOARD_Y ( (ui_screenheight - (8 * UI_SIZE_SQUARE_HEIGHT) ) / 2 )
#define UI_POS_FONT_OFFSET_Y ((-1 * 20 * (UI_SIZE_SQUARE_HEIGHT)) / 100)

#define UI_POS_INFO_X ( ui_board_pos_x == (UI_POS_BOARD_X_LEFT) ? \
		(UI_POS_BOARD_X_LEFT) + 8 * (UI_SIZE_SQUARE_WIDTH) + (UI_SPACING_X_INFO) : \
		(UI_SPACING_X_INFO) )

#define UI_FONT_FAMILY_PIECES "Chess Merida Unicode"
#define UI_FONT_SIZE_PIECES (75.0 * (UI_SIZE_SQUARE_HEIGHT)) / 100.0

#define UI_FONT_FAMILY_TEXT "FreeSans"
#define UI_FONT_SIZE_NAME 20.0
#define UI_FONT_SIZE_INFO 16.0
#define UI_FONT_SIZE_COORD 10.0

#define UI_SPACING_X_INFO 24
#define UI_SPACING_Y_INFO 14

/*******************************************************/

Display* ui_display = NULL;
Window ui_rootwindow = 0;
GC ui_gcontext = 0;
Pixmap ui_backbuf = 0;
XftFont* ui_piecefont = NULL;
XftFont* ui_infofont = NULL;
XftFont* ui_namefont = NULL;
XftFont* ui_coordfont = NULL;
XftDraw* ui_xftdraw = NULL;
XftColor ui_squarecolorwhite;
XftColor ui_squarecolorblack;
XftColor ui_piececolorwhite;
XftColor ui_piececolorblack;
XftColor ui_infocolor;
XftColor ui_infocolortag;
bool ui_squarecolorwhiteinit = false;
bool ui_squarecolorblackinit = false;
bool ui_piececolorwhiteinit = false;
bool ui_piececolorblackinit = false;
bool ui_infocolorinit = false;
bool ui_infocolortaginit = false;
int ui_screenwidth = -1;
int ui_screenheight = -1;
int ui_infoendposy = 0;
int ui_infofontchwidth = 0;
int ui_evalposy = 0;
int ui_board_pos_x = 0;

/*******************************************************/

static void ui_board_pos_to_pixel(int pos, int* x, int* y);
static void ui_piece_color(char piece, XftColor** color, XftColor** colorbg);
static void ui_piece_char(char piece, XftChar16* ch, XftChar16* chbg);
static void ui_draw_string( const char* str, XftFont* font, XftColor* color,
		int x, int y, int maxwidth, int* width);
static void ui_draw_info_string(const char* str, int x, int y,
		int maxwidth, int* width);
static void ui_draw_info_string_tag(const char* str, int x, int y,
		int maxwidth, int* width);
static void ui_draw_name_string(const char* str, int x, int y,
		int maxwidth, int* width);
static void ui_draw_tag_value(const char* tag, const char* value, XftFont* font,
		int x, int y, int maxwidth, int* width);
static void ui_draw_game_info_tag_value(const char* tag, const char* value,
		int x, int y, int maxwidth, int* width);
static void ui_draw_name_tag_value(const char* tag, const char* value,
		int x, int y, int maxwidth, int* width);
static void ui_draw_game_info_player(const char* tag, const char* value,
		const char* elostr, int x, int y, int maxwidth);
static void ui_draw_full_move_str(int x, int y, int maxwidth, int movenum,
		const char* whitepgn, const char* blackpgn);
static int ui_info_font_normal_char_width();
static void ui_draw_move_list(int miny, int maxy);
static void ui_rotate(XPoint* points, int npoints, int rx, int ry,
		double angle);
static void ui_draw_arrowhead(int xfrom, int yfrom, int xto, int yto);
static void ui_draw_coordinates();

/*******************************************************/

bool ui_init()
{
	movelist_init();

	/* window, drawable stuff */
	ui_display = XOpenDisplay(getenv("DISPLAY"));
	if (NULL == ui_display) {
		ui_close();
		return false;
	}

	ui_rootwindow = DefaultRootWindow(ui_display);
	if (0 == ui_rootwindow) {
		ui_close();
		return false;
	}

	ui_screenwidth = DisplayWidth(ui_display, DefaultScreen(ui_display));
	ui_screenheight = DisplayHeight(ui_display, DefaultScreen(ui_display));
	int depth = DisplayPlanes(ui_display, DefaultScreen(ui_display));

	LOG( INFO, "Screen size: %dx%d, Depth: %d", ui_screenwidth, ui_screenheight, depth );

	ui_backbuf = XCreatePixmap( ui_display, ui_rootwindow,
			ui_screenwidth, ui_screenheight, depth );

	if ( ui_backbuf == 0 ) {
		ui_close();
		return false;
	}

	ui_gcontext = XCreateGC(ui_display, ui_backbuf, 0, NULL);
	if (0 == ui_gcontext) {
		ui_close();
		return false;
	}

	ui_xftdraw = XftDrawCreate(ui_display, ui_backbuf, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0));
	if (NULL == ui_xftdraw) {
		ui_close();
		return false;
	}

	/* load fonts */
	ui_piecefont = XftFontOpen(ui_display, DefaultScreen(ui_display),
		XFT_FAMILY, XftTypeString, UI_FONT_FAMILY_PIECES,
		XFT_SIZE, XftTypeDouble, UI_FONT_SIZE_PIECES,
		NULL);

	if (NULL == ui_piecefont) {
		ui_close();
		return false;
	}

	ui_infofont = XftFontOpen(ui_display, DefaultScreen(ui_display),
		XFT_FAMILY, XftTypeString, UI_FONT_FAMILY_TEXT,
		XFT_SIZE, XftTypeDouble, UI_FONT_SIZE_INFO,
		NULL);

	if (NULL == ui_infofont) {
		ui_close();
		return false;
	}

	ui_namefont = XftFontOpen(ui_display, DefaultScreen(ui_display),
		XFT_FAMILY, XftTypeString, UI_FONT_FAMILY_TEXT,
		XFT_SIZE, XftTypeDouble, UI_FONT_SIZE_NAME,
		NULL);

	if (NULL == ui_namefont) {
		ui_close();
		return false;
	}

	ui_coordfont = XftFontOpen(ui_display, DefaultScreen(ui_display),
		XFT_FAMILY, XftTypeString, UI_FONT_FAMILY_TEXT,
		XFT_SIZE, XftTypeDouble, UI_FONT_SIZE_COORD,
		XFT_WEIGHT, XftTypeInteger, XFT_WEIGHT_BOLD,
		NULL);

	if (NULL == ui_coordfont) {
		ui_close();
		return false;
	}

	ui_infofontchwidth = ui_info_font_normal_char_width();

	/* create colors */
	XRenderColor whiteSquareColor;
	whiteSquareColor.red = (0xffff * ((UI_COL_WHITE_SQUARE >> 16) & 0xff)) / 0xff;
	whiteSquareColor.green = (0xffff * ((UI_COL_WHITE_SQUARE >> 8) & 0xff)) / 0xff;
	whiteSquareColor.blue = (0xffff * (UI_COL_WHITE_SQUARE & 0xff)) / 0xff;
	whiteSquareColor.alpha = 0xffff;

	if (!XftColorAllocValue(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0),
			&whiteSquareColor, &ui_squarecolorwhite)) {
		ui_squarecolorwhiteinit = false;
		ui_close();
		return false;
	}
	ui_squarecolorwhiteinit = true;

	XRenderColor blackSquareColor;
	blackSquareColor.red = (0xffff * ((UI_COL_BLACK_SQUARE >> 16) & 0xff)) / 0xff;
	blackSquareColor.green = (0xffff * ((UI_COL_BLACK_SQUARE >> 8) & 0xff)) / 0xff;
	blackSquareColor.blue = (0xffff * (UI_COL_BLACK_SQUARE & 0xff)) / 0xff;
	blackSquareColor.alpha = 0xffff;

	if (!XftColorAllocValue(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0),
			&blackSquareColor, &ui_squarecolorblack)) {
		ui_squarecolorblackinit = false;
		ui_close();
		return false;
	}
	ui_squarecolorblackinit = true;

	XRenderColor white;
	white.red = 0xffff;
	white.green = 0xffff;
	white.blue = 0xffff;
	white.alpha = 0xffff;

	if (!XftColorAllocValue(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0), &white, &ui_piececolorwhite)) {
		ui_piececolorwhiteinit = false;
		ui_close();
		return false;
	}
	ui_piececolorwhiteinit = true;

	XRenderColor black;
	black.red = 0;
	black.green = 0;
	black.blue = 0;
	black.alpha = 0xffff;

	if (!XftColorAllocValue(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0), &black, &ui_piececolorblack)) {
		ui_piececolorblackinit = false;
		ui_close();
		return false;
	}
	ui_piececolorblackinit = true;

	XRenderColor infocolor;
	infocolor.red = 0x6666;
	infocolor.green = 0x6666;
	infocolor.blue = 0x6666;
	infocolor.alpha = 0xffff;

	if (!XftColorAllocValue(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0), &infocolor, &ui_infocolor)) {
		ui_infocolorinit = false;
		ui_close();
		return false;
	}
	ui_infocolorinit = true;

	XRenderColor infocolortag;
	infocolortag.red = 0x4444;
	infocolortag.green = 0x4444;
	infocolortag.blue = 0x4444;
	infocolortag.alpha = 0xffff;

	if (!XftColorAllocValue(ui_display, DefaultVisual(ui_display, 0),
			DefaultColormap(ui_display, 0), &infocolortag, &ui_infocolortag)) {
		ui_infocolortaginit = false;
		ui_close();
		return false;
	}
	ui_infocolortaginit = true;

	/* start with board on the left */
	ui_board_pos_x = UI_POS_BOARD_X_LEFT;

	return true;
}

void ui_close()
{
	if (ui_squarecolorwhiteinit) {
		XftColorFree(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0), &ui_squarecolorwhite);
		ui_squarecolorwhiteinit = false;
	}
	if (ui_squarecolorblackinit) {
		XftColorFree(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0), &ui_squarecolorblack);
		ui_squarecolorblackinit = false;
	}
	if (ui_piececolorwhiteinit) {
		XftColorFree(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0), &ui_piececolorwhite);
		ui_piececolorwhiteinit = false;
	}
	if (ui_piececolorblackinit) {
		XftColorFree(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0), &ui_piececolorblack);
		ui_piececolorblackinit = false;
	}
	if (ui_infocolorinit) {
		XftColorFree(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0), &ui_infocolor);
		ui_infocolorinit = false;
	}
	if (ui_infocolortaginit) {
		XftColorFree(ui_display, DefaultVisual(ui_display, 0), DefaultColormap(ui_display, 0), &ui_infocolortag);
		ui_infocolortaginit = false;
	}

	if ( NULL != ui_xftdraw) {
		XftDrawDestroy(ui_xftdraw);
		ui_xftdraw = NULL;
	}

	if ( NULL != ui_piecefont) {
		XftFontClose(ui_display, ui_piecefont);
		ui_piecefont = NULL;
	}

	if ( NULL != ui_infofont) {
		XftFontClose(ui_display, ui_infofont);
		ui_infofont = NULL;
	}

	if ( NULL != ui_namefont) {
		XftFontClose(ui_display, ui_namefont);
		ui_namefont = NULL;
	}

	if ( NULL != ui_coordfont) {
		XftFontClose(ui_display, ui_coordfont);
		ui_coordfont = NULL;
	}

	if (0 != ui_gcontext) {
		XFreeGC(ui_display, ui_gcontext);
		ui_gcontext = 0;
	}

	if ( ui_backbuf != 0 ) {
		XFreePixmap( ui_display, ui_backbuf );
		ui_backbuf = 0;
	}

	ui_rootwindow = 0;

	if ( NULL != ui_display) {
		XCloseDisplay(ui_display);
		ui_display = NULL;
	}
}

void ui_flush()
{
	dbgutil_test(NULL != ui_display);

	XCopyArea( ui_display, ui_backbuf, ui_rootwindow, ui_gcontext,
			0, 0, ui_screenwidth, ui_screenheight, 0, 0 );

	XFlush(ui_display);
}

void ui_clear()
{
	(void) XSetForeground(ui_display, ui_gcontext, UI_COL_BACKGROUND);
	XFillRectangle(ui_display, ui_backbuf, ui_gcontext,
		0, 0, ui_screenwidth, ui_screenheight);

	movelist_clear();
}

void ui_toggle_board_position()
{
	int step_cnt = UI_POS_BOARD_X_RIGHT - UI_POS_BOARD_X_LEFT;
	int step = 0;
	if ( ui_board_pos_x == UI_POS_BOARD_X_LEFT ) {
		step = 1;
		ui_board_pos_x = UI_POS_BOARD_X_RIGHT;
	} else {

		step = -1;
		ui_board_pos_x = UI_POS_BOARD_X_LEFT;
	}
	for ( int i = 0; i < step_cnt; ++i ) {

		XCopyArea( ui_display, ui_backbuf, ui_rootwindow, ui_gcontext,
				0, 0 , ui_screenwidth, ui_screenheight, i * step, 0 );
		XFlush( ui_display );
		usleep( 800 );
	}
}

void ui_draw_game_info(const char* white, const char* black,
		const char* whiteelo, const char* blackelo,
		const char* event, const char* round,
		const char* site, const char* date,
		const char* eco, const char* ecoinfo)
{
	int x = UI_POS_INFO_X;
	int maxwidth = UI_SIZE_INFO_WIDTH;

	int y = UI_POS_BOARD_Y + UI_FONT_SIZE_NAME;
	int ystart = y;

	if (NULL != white && 0 < strlen(white)) {
		ui_draw_game_info_player("White:", white, whiteelo, x, y, maxwidth);
		y += UI_FONT_SIZE_NAME + UI_SPACING_Y_INFO;
	} else if (NULL != whiteelo && 0 < strlen(whiteelo)) {
		ui_draw_name_tag_value("WhiteElo:", whiteelo, x, y, maxwidth, NULL);
		y += UI_FONT_SIZE_NAME + UI_SPACING_Y_INFO;
	}

	if (NULL != black && 0 < strlen(black)) {
		ui_draw_game_info_player("Black:", black, blackelo, x, y, maxwidth);
		y += UI_FONT_SIZE_NAME + UI_SPACING_Y_INFO;
	} else if (NULL != blackelo && 0 < strlen(blackelo)) {
		ui_draw_name_tag_value("BlackElo:", blackelo, x, y, maxwidth, NULL);
		y += UI_FONT_SIZE_NAME + UI_SPACING_Y_INFO;
	}

	if (y != ystart) {
		y += UI_FONT_SIZE_INFO;
	}

	if (NULL != event && 0 < strlen(event)) {
		ui_draw_game_info_tag_value("Event:", event, x, y, maxwidth, NULL);
		y += UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO;
	}

	if (NULL != round && 0 < strlen(round)) {
		ui_draw_game_info_tag_value("Round:", round, x, y, maxwidth, NULL);
		y += UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO;
	}

	if (NULL != site && 0 < strlen(site)) {
		ui_draw_game_info_tag_value("Site:", site, x, y, maxwidth, NULL);
		y += UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO;
	}

	if (NULL != date && 0 < strlen(date)) {
		ui_draw_game_info_tag_value("Date:", date, x, y, maxwidth, NULL);
		y += UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO;
	}

	if (NULL != eco && 0 < strlen(eco)) {

		int	ecowidth = 0;
		ui_draw_game_info_tag_value("ECO:", eco, x, y, maxwidth, &ecowidth);

		if ( NULL != ecoinfo ) {
			int ecoinfowidth1 = 0;
			ui_draw_info_string( " (", x + ecowidth, y, maxwidth - ecowidth, &ecoinfowidth1 );
			int ecoinfowidth2 = 0;
			ui_draw_info_string( ecoinfo, x + ecowidth + ecoinfowidth1, y,
					maxwidth - ecowidth - ecoinfowidth1 - UI_SPACING_X_INFO, &ecoinfowidth2 );
			ui_draw_info_string( ")", x + ecowidth + ecoinfowidth1 + ecoinfowidth2, y,
					maxwidth - ecowidth - ecoinfowidth1 - ecoinfowidth2, NULL );
		}

		y += UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO;
	}

	/* update engine eval. start pos */
	ui_evalposy = y;

	y += UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO;

	/* update movelist start pos */
	ui_infoendposy = y;
}

void ui_draw_board()
{
	/* white squares */
	(void) XSetForeground(ui_display, ui_gcontext, UI_COL_WHITE_SQUARE);
	for (int y = 0; y < 8; ++y) {
		for (int x = (y % 2); x < 8; x += 2) {
			XFillRectangle(ui_display, ui_backbuf, ui_gcontext,
				ui_board_pos_x + x * UI_SIZE_SQUARE_WIDTH,
				UI_POS_BOARD_Y + y * UI_SIZE_SQUARE_HEIGHT,
				UI_SIZE_SQUARE_WIDTH, UI_SIZE_SQUARE_HEIGHT);
		}
	}

	/* draw black squares */
	(void) XSetForeground(ui_display, ui_gcontext, UI_COL_BLACK_SQUARE);
	for (int y = 0; y < 8; ++y) {
		for (int x = ((y + 1) % 2); x < 8; x += 2) {
			XFillRectangle(ui_display, ui_backbuf, ui_gcontext,
				ui_board_pos_x + x * UI_SIZE_SQUARE_WIDTH,
				UI_POS_BOARD_Y + y * UI_SIZE_SQUARE_HEIGHT,
				UI_SIZE_SQUARE_WIDTH, UI_SIZE_SQUARE_HEIGHT);
		}
	}

	/* coordinates */
	ui_draw_coordinates();
}

void ui_draw_piece(char piece, int pos)
{
	int x = 0;
	int y = 0;
	ui_board_pos_to_pixel(pos, &x, &y);

	/* top of square, text is aligned at bottom */
	y += UI_SIZE_SQUARE_HEIGHT;
	y += UI_POS_FONT_OFFSET_Y;

	XftChar16 piecechar;
	XftChar16 piececharbg;
	ui_piece_char(piece, &piecechar, &piececharbg);

	XftColor* color = NULL;
	XftColor* colorbg = NULL;
	ui_piece_color(piece, &color, &colorbg);

	XftDrawString16(ui_xftdraw, colorbg, ui_piecefont, x, y, &piececharbg, 1);
	XftDrawString16(ui_xftdraw, color, ui_piecefont, x, y, &piecechar, 1);
}

void ui_highlight_move(int from, int to)
{

	(void) XSetForeground(ui_display, ui_gcontext, UI_COL_HIGHLIGHT_SQUARE);
	(void) XSetLineAttributes(ui_display, ui_gcontext, UI_SIZE_HIGHLIGHT_LINE_WIDTH,
			LineSolid, CapNotLast, JoinMiter);

	int xfrom = 0;
	int yfrom = 0;
	ui_board_pos_to_pixel(from, &xfrom, &yfrom);
	int xto = 0;
	int yto = 0;
	ui_board_pos_to_pixel(to, &xto, &yto);
	if ( xfrom == xto ) {
		/* vertical */
		xto += UI_SIZE_SQUARE_WIDTH / 2;
		if ( yfrom > yto ) {
			yto += UI_SIZE_SQUARE_HEIGHT;
		}

	} else if ( yfrom == yto ) {
		/* horizontal */
		yto += UI_SIZE_SQUARE_HEIGHT / 2;
		if ( xfrom > xto ) {
			xto += UI_SIZE_SQUARE_WIDTH;
		}
	} else if ( abs(xto - xfrom) == abs(yto - yfrom) ) {
		/* diagonal */
		if ( xto < xfrom ) {
			xto += UI_SIZE_SQUARE_WIDTH;
		}

		if ( yto  < yfrom ) {
			yto += UI_SIZE_SQUARE_HEIGHT;
		}

	} else {
		/* knight */
		if ( abs(xto - xfrom) > abs(yto - yfrom) ) {

			if ( yto > yfrom ) {
				yto += UI_SIZE_SQUARE_HEIGHT / 3;
			} else {
				yto += (2 * UI_SIZE_SQUARE_HEIGHT) / 3;
			}

			if ( xfrom > xto ) {
				xto += UI_SIZE_SQUARE_WIDTH;
			}

		} else {
			if ( xto > xfrom ) {
				xto += UI_SIZE_SQUARE_WIDTH / 3;
			} else {
				xto += (2 * UI_SIZE_SQUARE_WIDTH) / 3;
			}

			if ( yfrom > yto ) {
				yto += UI_SIZE_SQUARE_HEIGHT;
			}
		}
	}

	xfrom += UI_SIZE_SQUARE_WIDTH / 2;
	yfrom += UI_SIZE_SQUARE_HEIGHT / 2;

	XDrawLine(ui_display, ui_backbuf, ui_gcontext, xfrom, yfrom, xto, yto);
	ui_draw_arrowhead(xfrom, yfrom, xto, yto);
}

void ui_draw_move_str(int movenum, bool white, const char* pgnstr)
{
	movelist_add_half_move(movenum, pgnstr, white);
	ui_draw_move_list( ui_infoendposy, UI_POS_BOARD_Y + 8 * UI_SIZE_SQUARE_HEIGHT );
}

void ui_draw_engine_eval( const char* scorestr, const char* evalstr )
{
	int x = UI_POS_INFO_X;
	const int maxwidth = UI_SIZE_INFO_WIDTH;

	(void) XSetForeground(ui_display, ui_gcontext, UI_COL_BACKGROUND);
	XFillRectangle(ui_display, ui_backbuf, ui_gcontext,
			x, ui_evalposy - UI_FONT_SIZE_INFO - UI_SPACING_Y_INFO / 2,
			maxwidth, UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO);

	int pixlen = 0;
	int accpixlen = 0;
	ui_draw_game_info_tag_value( "Eval:", scorestr, x, ui_evalposy, maxwidth, &pixlen );
	x += pixlen + ui_infofontchwidth;
	accpixlen += pixlen + ui_infofontchwidth;
	ui_draw_info_string( evalstr, x, ui_evalposy, maxwidth - accpixlen, &pixlen );
}

void ui_draw_result( const char* resultstr )
{
	const int x = UI_POS_INFO_X;
	const int maxwidth = UI_SIZE_INFO_WIDTH;

	const int miny = ui_infoendposy;
	const int maxy = UI_POS_BOARD_Y + 8 * UI_SIZE_SQUARE_HEIGHT;

	const int movestrheight = UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO;
	const int maxheight = maxy - miny;
	const int height = (((maxheight + (movestrheight / 2)) / movestrheight) * movestrheight);

	const int maxnboflines = height / movestrheight;
	const int maxnbofmoves = maxnboflines - 1;
	const int nbofmoves = (movelist_size() > maxnbofmoves) ? maxnbofmoves : movelist_size();

	const int y = miny + nbofmoves * movestrheight;

	ui_draw_move_list( ui_infoendposy, y );

	(void) XSetForeground(ui_display, ui_gcontext, UI_COL_BACKGROUND);
	XFillRectangle(ui_display, ui_backbuf, ui_gcontext,
			x, y, maxwidth, UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO);

	ui_draw_info_string( resultstr, x, y + UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO / 2, maxwidth, NULL);
}

/*******************************************************/

static void ui_board_pos_to_pixel(int pos, int* x, int* y)
{
	dbgutil_test(NULL != x);
	dbgutil_test(NULL != y);

	*x = ui_board_pos_x + UI_SIZE_SQUARE_WIDTH * (pos % 8);
	*y = UI_POS_BOARD_Y + UI_SIZE_SQUARE_HEIGHT * (7 - (pos / 8));
}

static void ui_piece_color(char piece, XftColor** color, XftColor** colorbg)
{
	// lower-case => black
	if ('a' < piece) {
		*color = &ui_piececolorblack;
		*colorbg = &ui_piececolorwhite;
	} else {
		*color = &ui_piececolorblack;
		*colorbg = &ui_piececolorwhite;
	}
}

static void ui_piece_char(char piece, XftChar16* ch, XftChar16* chbg)
{
	dbgutil_test(NULL != ch);

	// lower-case => black
	switch (piece) {
	case 'P':
		*ch = 0x2659;
		*chbg = 0x265f;
		return;
	case 'p':
		*ch = 0x265f;
		*chbg = 0x265f;
		return;
	case 'N':
		*ch = 0x2658;
		*chbg = 0xe258;
		return;
	case 'n':
		*ch = 0x265e;
		*chbg = 0xe258;
		return;
	case 'B':
		*ch = 0x2657;
		*chbg = 0xe257;
		return;
	case 'b':
		*ch = 0x265d;
		*chbg = 0xe257;
		return;
	case 'R':
		*ch = 0x2656;
		*chbg = 0xe256;
		return;
	case 'r':
		*ch = 0x265c;
		*chbg = 0xe256;
		return;
	case 'Q':
		*ch = 0x2655;
		*chbg = 0xe255;
		return;
	case 'q':
		*ch = 0x265b;
		*chbg = 0xe255;
		return;
	case 'K':
		*ch = 0x2654;
		*chbg = 0xe254;
		return;
	case 'k':
		*ch = 0x265a;
		*chbg = 0xe254;
		return;
	default:
		*ch = 0x0;
		*chbg = 0x0;
		return;
	}
}

static void ui_draw_string( const char* str, XftFont* font, XftColor* color, int x, int y, int maxwidth, int* width)
{
	XftChar16 dotch = 0x2026; // unicode ... character

	/* get width of three dots */
	XGlyphInfo extents;
	XftTextExtents16(ui_display, font, &dotch, 1, &extents);

	int sizepoints = extents.width;

	size_t cutlen = 0;

	XftTextExtentsUtf8(ui_display, font, (XftChar8*) str, strlen(str), &extents);

	int totlen = extents.width;

	while (totlen > maxwidth && cutlen < strlen(str)) {
		++cutlen;

		XftTextExtentsUtf8(ui_display, font, (XftChar8*) str, strlen(str) - cutlen, &extents);
		totlen = extents.width + sizepoints;
	}

	if (NULL != width) {
		*width = 0;
	}

	if (totlen <= maxwidth) {
		XftDrawStringUtf8(ui_xftdraw, color, font, x, y, (XftChar8*) str, strlen(str) - cutlen);

		if (cutlen > 0) {
			XftDrawString16(ui_xftdraw, color, font, x + extents.width, y, &dotch, 1);
		}

		if (NULL != width) {
			*width = totlen;
		}
	}
}

static void ui_draw_info_string(const char* str, int x, int y, int maxwidth, int* width)
{
	ui_draw_string( str, ui_infofont, &ui_infocolor, x, y, maxwidth, width);
}

static void ui_draw_info_string_tag(const char* str, int x, int y, int maxwidth, int* width)
{
	ui_draw_string( str, ui_infofont, &ui_infocolortag, x, y, maxwidth, width);
}

static void ui_draw_name_string(const char* str, int x, int y, int maxwidth, int* width)
{
	ui_draw_string( str, ui_namefont, &ui_infocolor, x, y, maxwidth, width);
}

static void ui_draw_tag_value(const char* tag, const char* value, XftFont* font,
		int x, int y, int maxwidth, int* width)
{
	int accx = x;

	int tagwidth = 0;
	ui_draw_string(tag, font, &ui_infocolortag, accx, y, maxwidth, &tagwidth);

	accx += tagwidth;

	XGlyphInfo extents;
	XftTextExtents8(ui_display, font, (XftChar8*) "x", 1, &extents);

	accx += extents.width;

	int valuewidth = 0;
	ui_draw_string(value, font, &ui_infocolor, accx, y, maxwidth - (accx - x), &valuewidth);

	accx += valuewidth;

	if (NULL != width) {
		*width = accx - x;
	}
}

static void ui_draw_game_info_tag_value(const char* tag, const char* value, int x, int y, int maxwidth, int* width)
{
	ui_draw_tag_value( tag, value, ui_infofont, x, y, maxwidth, width );
}

static void ui_draw_name_tag_value(const char* tag, const char* value, int x, int y, int maxwidth, int* width)
{
	ui_draw_tag_value( tag, value, ui_namefont, x, y, maxwidth, width );
}

static void ui_draw_game_info_player(const char* tag, const char* value, const char* elostr, int x, int y, int maxwidth)
{
	int tagvaluewidth = 0;
	ui_draw_name_tag_value(tag, value, x, y, maxwidth, &tagvaluewidth);

	if (NULL != elostr && 0 < strlen(elostr) && tagvaluewidth < maxwidth) {

		size_t tmpstrlen = 3 + strlen(elostr) + 1;
		char* tmpstr = malloc(tmpstrlen * sizeof(char));

		snprintf(tmpstr, tmpstrlen, " (%s)", elostr);

		ui_draw_name_string(tmpstr, x + tagvaluewidth, y, maxwidth - tagvaluewidth, NULL);

		free(tmpstr);
		tmpstr = NULL;

	}
}

static void ui_draw_full_move_str(int x, int y, int maxwidth, int movenum, const char* whitepgn, const char* blackpgn)
{
	int numwidth = 0;

	if (movenum > 0) {
		char numstr[8];
		snprintf(numstr, 8, "%d.", movenum);
		ui_draw_info_string_tag(numstr, x, y, maxwidth, NULL);

		int numcnt = strlen(numstr) > 7 ? strlen(numstr) : 7;
		numwidth =  numcnt * ui_infofontchwidth;
	}

	x += numwidth;
	maxwidth -= numwidth;

	int whitewidth = 0;
	if (NULL != whitepgn && '\0' != (*whitepgn)) {
		ui_draw_info_string(whitepgn, x, y, maxwidth, &whitewidth);
	} else {
		XftChar16 dotch = 0x2026; // unicode ... character

		/* get width of three dots */
		XGlyphInfo extents;
		XftTextExtents16(ui_display, ui_infofont, &dotch, 1, &extents);

		whitewidth = extents.width;

		XftDrawString16(ui_xftdraw, &ui_infocolor, ui_infofont, x, y, &dotch, 1);
	}

	whitewidth = whitewidth < (ui_infofontchwidth * 8) ? (ui_infofontchwidth * 8) : whitewidth;

	x += whitewidth + UI_SPACING_X_INFO;
	maxwidth -= whitewidth + UI_SPACING_X_INFO;

	if (NULL != blackpgn && '\0' != (*blackpgn)) {
		ui_draw_info_string(blackpgn, x, y, maxwidth, NULL);
	}
}

static int ui_info_font_normal_char_width()
{
	XftChar8 ch = 'a';

	XGlyphInfo extents;
	XftTextExtents8(ui_display, ui_infofont, &ch, 1, &extents);

	return extents.width;
}

static void ui_draw_move_list( int miny, int maxy )
{
	int x = UI_POS_INFO_X;
	const int maxwidth = UI_SIZE_INFO_WIDTH;

	const int movestrheight = UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO;
	const int maxheight = maxy - miny;
	const int height = (((maxheight + (movestrheight / 2)) / movestrheight) * movestrheight);

	const int maxnboflines = height / movestrheight;
	const int maxnbofmoves = maxnboflines;
	const int startmove = (movelist_size() > maxnbofmoves) ? (movelist_size() - maxnbofmoves) : 0;

	int y = miny;

	(void) XSetForeground(ui_display, ui_gcontext, UI_COL_BACKGROUND);
	XFillRectangle(ui_display, ui_backbuf, ui_gcontext,
			x, y, maxwidth, height);

	for (int i = startmove; i < movelist_size(); ++i) {

		ui_draw_full_move_str(x, y + UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO / 2, maxwidth,
				movelist_get_move_num(i), movelist_get_white(i), movelist_get_black(i));

		y += UI_FONT_SIZE_INFO + UI_SPACING_Y_INFO;
	}
}

static void ui_rotate(XPoint* points, int npoints, int rx, int ry, double angle)
{
    for (int i=0; i<npoints; ++i) {
		double x = points[i].x - rx;
		double y = points[i].y - ry;
		points[i].x =  rx + x * cos(angle) - y * sin(angle);
		points[i].y =  ry + x * sin(angle) + y * cos(angle);
    }
}

static void ui_draw_arrowhead(int xfrom, int yfrom, int xto, int yto)
{
	XPoint arrowhead[5];
	arrowhead[0].x = -5;
	arrowhead[0].y = 0;

	arrowhead[1].x = -10;
	arrowhead[1].y = 15;

	arrowhead[2].x = 7;
	arrowhead[2].y = 0;

	arrowhead[3].x = -10;
	arrowhead[3].y = -15;

	arrowhead[4].x = -5;
	arrowhead[4].y = 0;

	/* calculate angle for vector */
	double angle = atan2( yto - yfrom, xto - xfrom );

	ui_rotate(arrowhead, 5, 0, 0, angle);

	for (int i = 0; i < 5; ++i) {
		arrowhead[i].x += xto;
		arrowhead[i].y += yto;
	}

	(void) XSetForeground(ui_display, ui_gcontext, UI_COL_HIGHLIGHT_SQUARE);
	(void) XFillPolygon(ui_display, ui_backbuf, ui_gcontext, arrowhead, 5, Complex, CoordModeOrigin);
}

static void ui_draw_coordinates()
{
	for ( int y = 0; y < 8; ++y ) {

		XftChar8 c = '8' - y;
		XGlyphInfo extents;
		XftTextExtentsUtf8(ui_display, ui_coordfont, &c, 1, &extents);

		int xPos = ui_board_pos_x + (4 * UI_SIZE_SQUARE_WIDTH) / 100;
		int yPos = UI_POS_BOARD_Y + (UI_SIZE_SQUARE_HEIGHT * y) +
				extents.height + (4 * UI_SIZE_SQUARE_WIDTH) / 100;

		XftColor* color = NULL;
		if ( y % 2 == 0 ) {
			color = &ui_squarecolorblack;
		} else {
			color = &ui_squarecolorwhite;
		}

		XftDrawStringUtf8(ui_xftdraw, color, ui_coordfont, xPos, yPos, &c, 1);
	}

	for ( int x = 0; x < 8; ++x ) {

		XftChar8 c = 'a' + x;
		XGlyphInfo extents;
		XftTextExtentsUtf8(ui_display, ui_coordfont, &c, 1, &extents);

		int xPos = ui_board_pos_x + (UI_SIZE_SQUARE_WIDTH * (x + 1)) -
				extents.width;// - (4 * UI_SIZE_SQUARE_WIDTH) / 100;
		int yPos = UI_POS_BOARD_Y + (UI_SIZE_SQUARE_HEIGHT * 8) -
				(60 * extents.height) / 100;

		XftColor* color = NULL;
		if ( x % 2 == 0 ) {
			color = &ui_squarecolorwhite;
		} else {
			color = &ui_squarecolorblack;
		}

		XftDrawStringUtf8(ui_xftdraw, color, ui_coordfont, xPos, yPos, &c, 1);
	}
}

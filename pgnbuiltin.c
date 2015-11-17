#include "pgnbuiltin.h"
#include "dbgutil.h"

#include <string.h>


const char* pgnbuiltin_data =
	"[Event \"Hoogovens A Tournament\"]\n"
	"[Site \"Wijk aan Zee NED\"]\n"
	"[Date \"1999.01.20\"]\n"
	"[EventDate \"?\"]\n"
	"[Round \"4\"]\n"
	"[Result \"1-0\"]\n"
	"[White \"Garry Kasparov\"]\n"
	"[Black \"Veselin Topalov\"]\n"
	"[ECO \"B06\"]\n"
	"[WhiteElo \"2812\"]\n"
	"[BlackElo \"2700\"]\n"
	"[PlyCount \"87\"]\n"
	"\n"
	"1. e4 d6 2. d4 Nf6 3. Nc3 g6 4. Be3 Bg7 5. Qd2 c6 6. f3 b5 "
	"7. Nge2 Nbd7 8. Bh6 Bxh6 9. Qxh6 Bb7 10. a3 e5 11. O-O-O Qe7 "
	"12. Kb1 a6 13. Nc1 O-O-O 14. Nb3 exd4 15. Rxd4 c5 16. Rd1 Nb6 "
	"17. g3 Kb8 18. Na5 Ba8 19. Bh3 d5 20. Qf4+ Ka7 21. Rhe1 d4 "
	"22. Nd5 Nbxd5 23. exd5 Qd6 24. Rxd4 cxd4 25. Re7+ Kb6 "
	"26. Qxd4+ Kxa5 27. b4+ Ka4 28. Qc3 Qxd5 29. Ra7 Bb7 30. Rxb7 "
	"Qc4 31. Qxf6 Kxa3 32. Qxa6+ Kxb4 33. c3+ Kxc3 34. Qa1+ Kd2 "
	"35. Qb2+ Kd1 36. Bf1 Rd2 37. Rd7 Rxd7 38. Bxc4 bxc4 39. Qxh8 "
	"Rd3 40. Qa8 c3 41. Qa4+ Ke1 42. f4 f5 43. Kc1 Rd2 44. Qa7 1-0\n";

static int pgnbuiltin_pos = 0;

/****************************************************************************/

int pgnbuiltin_cnt()
{
	return strlen( pgnbuiltin_data );
}

char pgnbuiltin_get()
{
	dbgutil_test( pgnbuiltin_pos >= 0 );
	dbgutil_test( pgnbuiltin_pos < pgnbuiltin_cnt() );

	char c = pgnbuiltin_data[pgnbuiltin_pos];
	pgnbuiltin_pos++;
	if ( pgnbuiltin_pos >= pgnbuiltin_cnt() ) {
		pgnbuiltin_pos = 0;
	}

	return c;
}

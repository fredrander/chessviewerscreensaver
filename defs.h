#ifndef __defs_h__
#define __defs_h__

#define CW_NB_OF_FILES 8
#define CW_NB_OF_RANKS 8
#define CW_NB_OF_SQUARES (CW_NB_OF_FILES * CW_NB_OF_RANKS)

#define CW_NO_PIECE '\0'

/* max string length for a pgn move, inkl. null term. */
/* ex. Qa5xd4+ */
#define CW_MAX_MOVE_STRING 8

/* max string length for a long algebraic move, inkl. null term. */
/* ex. a7a8Q */
#define CW_MAX_LONG_ALGEBRAIC_STRING 6

#endif /* __defs_h__ */

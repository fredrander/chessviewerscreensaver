CC=gcc
CFLAGS=-std=c11 -I/usr/include/freetype2
LIBS=-lX11 -lXft -lfontconfig -lpthread -lm
DEPS = *.h *.c
OBJ = main.o ui.o pgn.o pgnparser.o chess.o log.o engine.o popen2.o movelist.o eco.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

chessviewer: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

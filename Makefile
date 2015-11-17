APPLICATION=chessviewer
CC=gcc
CFLAGS=-std=c11 -I/usr/include/freetype2
LIBS=-lX11 -lXft -lfontconfig -lpthread -lm
DEPS = *.h *.c
OBJ = main.o ui.o pgn.o pgnparser.o chess.o log.o engine.o popen2.o movelist.o eco.o cmdline.o ecodb.o pgnbuiltin.o


all: $(APPLICATION)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(APPLICATION): $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

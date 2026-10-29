CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

.PHONY: all limpiar

all: servidor cliente

servidor: server.c blackjack.c blackjack.h protocol.c protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ server.c blackjack.c protocol.c $(LDLIBS)

cliente: client.c ui.c ui.h blackjack.h protocol.c protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ client.c ui.c protocol.c $(LDLIBS) -lncurses

limpiar:
	rm -f servidor cliente servidor.exe cliente.exe

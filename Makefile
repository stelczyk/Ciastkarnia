CC = gcc
CFLAGS = -Wall -g

all: kierownik piekarz klient kasjer

kierownik: kierownik.c ciastkarnia.h
	$(CC) $(CFLAGS) -o kierownik kierownik.c

piekarz: piekarz.c ciastkarnia.h
	$(CC) $(CFLAGS) -o piekarz piekarz.c

klient: klient.c ciastkarnia.h
	$(CC) $(CFLAGS) -o klient klient.c

kasjer: kasjer.c ciastkarnia.h
	$(CC) $(CFLAGS) -o kasjer kasjer.c

clean:
	rm -f kierownik piekarz klient kasjer

run: all
	./kierownik
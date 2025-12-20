CC = gcc
CFLAGS = -Wall -g

all: kierownik piekarz klient

kierownik: kierownik.c ciastkarnia.h
	$(CC) $(CFLAGS) -o kierownik kierownik.c

piekarz: piekarz.c ciastkarnia.h
	$(CC) $(CFLAGS) -o piekarz piekarz.c

klient: klient.c ciastkarnia.h
	$(CC) $(CFLAGS) -o klient klient.c

clean:
	rm -f kierownik piekarz klient

run: all
	./kierownik
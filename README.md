# Symulacja Ciastkarni

- **Przedmiot:** Systemy Operacyjne
- **Distribution:** Ubuntu 24.04
- **Virtualization:** wsl
- **Kernel:** Linux 6.6.87.2-microsoft-standard-WSL2
- **Compilator:** gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0

---


## Kompilacja i uruchomienie

### Kompilacja

```
make clean && make
```

Makefile:
```makefile
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
```

### Uruchomienie

```bash
./kierownik
```
---
## Wprowadzenie
- Celem projektu bylo zrobiebie symulacji ciastkarni w srodowisku systemu Linux
- Program jest zdecentralizowany, opiera się  na wielu współpracujących ze sobą procesach (piekarz, klient, kasjer, kierownik)
- Do tworzenia procesow potomnych wykorzystywane sa funkcje fork() oraz exec()

---

## Opis kodu

### kierownik.c
- Inicjalizuje zasoby IPC (pamiec dzielona, semafory, kolejki komunikatow)
- Uruchamia procesy potomne
- Generuje procesy klientow w petli przez czas pracy sklepu
- Obsluguje sygnaly (sprzatanie, inwentaryzajca, ewakuacja)
- Zarzadza cyklem zycia sklepu
- Generuje raport przy inwentaryzacji
- Sprzata zasoby

### piekarz.c
- Laczy sie z zasobami IPC
- Produkuje losowe ilosci losowych rodzajow produktow w petli
- Wysyla produkty na podajniki
- Aktualizuje statystyki produkcji
- Reaguje na sygnaly

### kasjer.c
- Laczy sie z zasobami IPC
- Odbiera koszyki klientow
- Generuje paragon z lista produktow i suma
- Wysyla potiwerdzenie obsluzenia
- Reaguje na otwieranie/zamykanie kasy
- Aktualizuje statystyki obsluzonych klientow
- Reaguje na sygnaly

### klient.c
- Laczy sie z zasobami IPC
- Sprawdza mozliwosc wejscia
- Symuluje zakupy
- Przy ewakuacji opuszcza towar do kosza
- Wybiera kase
- Opuszcza sklep, aktualizujac liczniki klientow i zwalnia semafory

---

## Zasoby IPC
- **Pamiec dzielona** Przechowuje stan globalny w strukturze DaneWspolne. Zawiera stany produkcji, sprzedazy, na podajnikach, liczbe klientow, flagi sterujacew, pidy i statusy kasjerow
- **Semafory** Zestaw semaforow




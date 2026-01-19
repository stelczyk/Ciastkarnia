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
- Celem projektu bylo zrobienie symulacji ciastkarni w srodowisku systemu Linux
- Program jest zdecentralizowany, opiera sie  na wielu wspolpracujacych ze soba procesach (piekarz, klient, kasjer, kierownik)
- Do tworzenia procesow potomnych wykorzystywane sa funkcje fork() oraz exec()

---

## Opis kodu

### kierownik.c
- Inicjalizuje zasoby IPC (pamiec dzielona, semafory, kolejki komunikatow)
- Uruchamia procesy potomne
- Generuje procesy klientow w petli przez czas pracy sklepu
- Obsluguje sygnaly (sprzatanie, inwentaryzacja, ewakuacja)
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
- Wysyla potwierdzenie obsluzenia
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

## Zasoby IPC oraz elementy kluczowe

### IPC

- **Pamiec dzielona** Przechowuje stan globalny w strukturze DaneWspolne. Zawiera stany produkcji, sprzedazy, na podajnikach, liczbe klientow, flagi sterujace
- **Semafory** Zestaw semaforow ktory chroni sekcje krytyczne
- **Kolejka komunikatow** Sluzy do komunikacji miedzy kasjer - klient

### Wybor kasy klienta

```
SEKCJA_KRYTYCZNA (SEM_MUTEX + SEM_KOLEJKA_KASA1 + SEM_KOLEJKA_KASA2):
  JESLI kasa2_otwarta:
    JESLI kolejka2 < kolejka1:
      wybrana = 2
    INACZEJ:
      wybrana = 1
  INACZEJ:
    wybrana = 1
  
  dane->kasa_kolejka[wybrana-1]++
  pozycja = dane->kasa_kolejka[wybrana-1]
KONIEC_SEKCJI

Wypisz "Ide do kasy {wybrana}, pozycja {pozycja}"
```


### Zarzadzanie kasa 2

```
// OTWIERANIE (klient wchodzi)
SEKCJA_KRYTYCZNA:
  dane->klienci_w_sklepie++
  
  JESLI klienci >= PROG_DRUGIEJ_KASY I !kasa_otwarta[1]:
    kasa_otwarta[1] = 1
    ODBLOKUJ(SEM_DZIALANIE_KASY2)  // obudz kasjera 2

// ZAMYKANIE (klient wychodzi)
SEKCJA_KRYTYCZNA:
  dane->klienci_w_sklepie--
  
  JESLI klienci < PROG_DRUGIEJ_KASY I kasa_otwarta[1] I !ewakuacja:
    kasa_otwarta[1] = 0

// KASJER 2 - reakcja
WHILE sklep_otwarty OR kolejka > 0:
  // Wykryj zmiane statusu
  JESLI status_sie_zmienil:
    Wypisz "otwieram" lub "zamykam"
  
  // Czekaj gdy zamknieta i brak kolejki
  JESLI !kasa_otwarta[1] I kolejka == 0:
    ZABLOKUJ(SEM_DZIALANIE_KASY2)
```

## Elementy wyrozniajace i problemy

### Elementy

**Synchronizacja wypisywania -** Wiele procesow wypisywalo jednoczesnie, co powodowalo przeplecione i nieczytelne logi.Zostalo to rozwiazane
za pomoca semafora SEM_OUTPUT ktory chroni operacje wypisywania.

**Kolorowane logi -** Kazdy typ procesu ma swoj wlasny kolor w terminalu

### Problemy

**Procesy zombie** Procesy potomne zostawaly w stanie zombie. 
```c
signal(SIGCHLD, SIG_IGN);
```
Zakonczone procesy potomne sa automatycznie sprzatane

**Ginace produkty w trakcie ewakuacji** Produkty ktore klienci mieli w koszyku nie byly zostawiane przy kasie
 Dodano tablice `w_koszu` 
 ```c
for(int i = 0; i < 12; i++) {
    dane->w_koszu[i] += koszyk[i];
    dane->sprzedano[i] -= koszyk[i];
}
```

## Test Obciazenia (Duza liczba klientow)

### Cel
Sprawdzenie, czy system wytrzymuje duza liczbe klientow probujacych wejsc do sklepu jednoczesnie.

```c
#define MAX_KLIENTOW 1000 
```
### Oczekiwany rezultat
- System nie ulega awarii, , semafor `SEM_WEJSCIE_SKLEP` poprawnie limituje liczbe klientow w sklepie do MAX_KLIENTOW

### Rezultat
**Zaliczony**


## Test Otwarcia Drugiej Kasy i zamkniecia drugiej kasy

### Konfiguracja
```c
#define MAX_KLIENTOW 16
#define PROG_DRUGIEJ_KASY (MAX_KLIENTOW/2)  
#define CZAS_PRACY_SKLEPU 30
```

### Cel
Sprawdzenie, czy druga kasa sie otwiera i zamyka automatycznie przy odpowiedniej liczbie klientow.

### Oczekiwany rezultat
- Kasa 1 jest otwarta od poczatku, gdy liczba klientow w sklepie przekroczy PROG_DRUGIEJ_KASY, kasa 2 powinna sie otworzyc

### Rezultat

```c
[KLIENT 385989] Wchodze do sklepu (klientow: 8/16)
[SYSTEM] Otwieram kase 2,   Klientow: 8 >= 8
```

```c
[KLIENT 386031] Wychodze (zostalo klientow: 7)
[SYSTEM] Zamykam kase 2,   Klientow: 7 < 8
```
**zaliczony**


## Test wyboru Kasy przez Klienta

### Cel
Sprawdzenie, czy klient wybiera kase z krotsza kolejka.

## Rezultat

```c
[KLIENT 385994] Ide do kasy 1, pozycja w kolecje 1
[KLIENT 386007] Brak Drozdzowka na podajniku!
[KLIENT 386011] Wchodze do sklepu (klientow: 14/16)
[KLIENT 386011] Chce kupic:
  - Mazurek: 3 szt.  - Babeczka: 3 szt.
[KLIENT 386001] Brak Tiramisu na podajniku!
[KLIENT 385995] Wychodze (zostalo klientow: 13)
[KLIENT 386011] Robie zakupy
[KLIENT 386001] Skonczylem zakupy, w koszyku mam:  (nic)  SUMA: 0 zl

[KLIENT 386008] Brak Szarlotka na podajniku!
[PIEKARZ 384516] Wyslalem Rogalik #1336 na podajnik (2/57)
[KLIENT 386009] Brak Paczek na podajniku!
[KLIENT 386004] Brak Wuzetka na podajniku!
[KLIENT 386006] Brak Ptys na podajniku!
[KLIENT 386012] Wchodze do sklepu (klientow: 14/16)
[KLIENT 386004] Skonczylem zakupy, w koszyku mam:  (nic)  SUMA: 0 zl

[KLIENT 386006] Skonczylem zakupy, w koszyku mam:  (nic)  SUMA: 0 zl

[KLIENT 386012] Chce kupic:
  - Paczek: 1 szt.  - Sernik: 2 szt.  - Mazurek: 1 szt.  - Tiramisu: 1 szt.
[KLIENT 386010] Brak Eklerek na podajniku!
[KLIENT 386002] Ide do kasy 2, pozycja w kolecje 1
[KLIENT 385998] Ide do kasy 1, pozycja w kolecje 2
[KLIENT 386000] Ide do kasy 2, pozycja w kolecje 2
[KLIENT 386005] Ide do kasy 1, pozycja w kolecje 3
```
**zaliczony**


## Test pelnego sklepu

### Cel
Sprawdzenie, czy jak sklep jest pelny, to klient czeka przed sklepem

### Rezultat

```c
[KLIENT 386014] Wchodze do sklepu (klientow: 16/16)
[KLIENT 386013] Robie zakupy
[KLIENT 386014] Chce kupic:
  - Szarlotka: 2 szt.  - Ptys: 1 szt.
[KLIENT 386011] Brak Mazurek na podajniku!
[KLIENT 386027] Sklep pelny (16/16), czekam przed sklepem...
```
**zaliczony**


## Test braku produktu na podajniku

### Cel
Sprawdzenie, czy klient omija produkt jak go nie ma na podajniku

### Rezultat

```c
[KLIENT 386025] Wchodze do sklepu (klientow: 15/16)
[KLIENT 386025] Chce kupic:
  - Paczek: 1 szt.  - Eklerek: 3 szt.  - Mazurek: 3 szt.  - Ptys: 3 szt.
  [KLIENT 386025] Brak Paczek na podajniku!
  [KLIENT 386025] Wzialem Eklerek #1341
  [KLIENT 386025] Wzialem Eklerek #1342
  [KLIENT 386025] Wzialem Eklerek #1343
  [KLIENT 386025] Brak Mazurek na podajniku!
  [KLIENT 386025] Brak Ptys na podajniku!
  [KLIENT 386025] Skonczylem zakupy, w koszyku mam:  - Eklerek: 3 szt.  SUMA: 15 zl
  [KASJER 384517] Kasa 1, Obsluguje klienta 386025
  Eklerek x3 = 15 zl
  SUMA: 15 zl (3 szt.) - Dziekujemy
  ```
**zaliczony**


## Test posprzatania zasobow

### Cel
Sprawdzenie czy po wyslaniu sygnalu SIGINT symulacja usuwa wszystkie zasoby z ktorych korzystala

### Rezultat

```c
[KLIENT 388698] Robie zakupy
^C

[KIEROWNIK] Otrzymano sygnal zakonczenia
[KIEROWNIK] Zamykam sklep i zwalniam pracownikow...
[KIEROWNIK] Odlaczono pamiec dzielona
[KIEROWNIK] Usunieto pamiec dzielona
[KIEROWNIK] Usunieto kolejke kas
[KIEROWNIK] Usunieto grupe semaforowa
[KIEROWNIK] Sklep zamkniety. Do widzenia.
```

```c
ipcs -a

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      

------ Semaphore Arrays --------
key        semid      owner      perms      nsems
```
**zaliczony**


## Test sygnalu inwentaryzacji

### Cel
Sprawdzenie, czy system poprawnie generuje raport po otrzymaniu sygnalu inwentaryzacji

### Rezultat 

```c
[KLIENT 426374] Wychodze (zostalo klientow: 0)
[KIEROWNIK] Wszyscy klienci wyszli, zamykam sklep
[KIEROWNIK] Laczna liczba stworzonych klientow: 18824
[KIEROWNIK] Generuje raport
[KIEROWNIK] Raport zapisano do: raport.txt
```

```
RAPORT INWENTARYZACJI

WYPRODUKOWANO:
Paczek: 580 szt.
Sernik: 459 szt.
Szarlotka: 496 szt.
Drozdzowka: 576 szt.
Kremowka: 627 szt.
Eklerek: 584 szt.
Mazurek: 535 szt.
Wuzetka: 540 szt.
Rogalik: 558 szt.
Ptys: 554 szt.
Babeczka: 511 szt.
Tiramisu: 519 szt.

NA PODAJNIKACH:
Drozdzowka: 3 szt.

SPRZEDANO:
Paczek: 580 szt.
Sernik: 459 szt.
Szarlotka: 496 szt.
Drozdzowka: 573 szt.
Kremowka: 627 szt.
Eklerek: 584 szt.
Mazurek: 535 szt.
Wuzetka: 540 szt.
Rogalik: 558 szt.
Ptys: 554 szt.
Babeczka: 511 szt.
Tiramisu: 519 szt.

KASY:
Kasa 1: 2723 klientow
Kasa 2: 1130 klientow
```
**zaliczony**


## Test sygnalu ewakuacji

### Cel
Sprawdzenie, czy ewakuacja zatrzymuje wszystkie operacje

### Rezultat

```c
[KIEROWNIK] Otrzymano sygnal zakonczenia
[KIEROWNIK] Zamykam sklep i zwalniam pracownikow...
[KLIENT 430507] EWAKUACJA! Nie wchodze do sklepu.
[KLIENT 430503] EWAKUACJA! Odkladam towar do kosza:   (koszyk byl pusty)
[KLIENT 430503] EWAKUACJA - wychodze! (zostalo klientow: 11)
.
.
.
[KLIENT 430501] Wzialem tylko 2/3 szt. Wuzetka
[KLIENT 430501] EWAKUACJA! Odkladam towar do kosza:   - Wuzetka: 2 szt.
[KLIENT 430501] EWAKUACJA - wychodze! (zostalo klientow: 0)
```
**zaliczony**

---

## Linki do istotnych fragmentow kodu


### a. Tworzenie i obsluga plikow
- **open()**: [kierownik.c:37](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L37)  
- **write()**: [kierownik.c:88](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L88) 
- **close()**: [kierownik.c:91](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L91)  


### b. Tworzenie procesow
- **fork()**: [kierownik.c:242](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L242)  
- **exec() (execl)**: [kierownik.c:248](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L248)
- **exit()**: [kierownik.c:158](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L158)  
- **wait()**: [kierownik.c:114](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L114)  


### d. Obsluga sygnalow
- **kill()**: [kierownik.c:31](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L31) 
- **signal()**: [kierownik.c:162](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L162) 


### e. Synchronizacja procesow
- **ftok()**: [kierownik.c:178](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L178) 
- **semget()**: [ciastkarnia.h:223](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/ciastkarnia.h#L223) 
- **semctl()**: [kierownik.c:151](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L151) 
- **semop()**: [ciastkarnia.h:161](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/ciastkarnia.h#L161) 


### g. Segmenty pamieci dzielonej
- **shmget()**: [kierownik.c:184](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L184) 
- **shmat()**: [kierownik.c:190](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L190) 
- **shmdt()**: [kierownik.c:129](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L129) 
- **shmctl()**: [kierownik.c:136](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L136) 

### h. Kolejki komunikatow
- **msgget()**: [kierownik.c:237](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L237) 
- **msgsnd()**: [klient.c:387](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/klient.c#L387) 
- **msgrcv()**: [kasjer.c:92](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kasjer.c#L92)  
- **msgctl()**: [kierownik.c:144](https://github.com/stelczyk/Ciastkarnia/blob/595dfb67bda80ead78de2831b753c2e2b965b0a5/kierownik.c#L144) 


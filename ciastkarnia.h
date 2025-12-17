#ifndef CIASTKARNIA_H
#define CIASTKARNIA_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

#define LICZBA_RODZAJOW 12
#define MAX_KLIENTOW 8
#define MAX_POJEMNOSC 20
#define PROG_DRUGIEJ KASY 4 
#define SCIEZKA_KLUCZA "/tmp"
#define PROJ_ID_SHM 'S'


static const char *NAZWA_PRODUKTOW[] = {
    "Pączek", "Sernik", "Szarlotka", "Drożdżówka", 
    "Kremówka", "Eklerek", "Mazurek", "Wuzetka", 
    "Rogalik", "Ptys", "Babeczka", "Tiramisu"
};

static const int CENY[] = {3, 15, 12, 4, 6, 5, 8, 7, 3, 5, 4, 10};

typedef struct {
    int podajnik[LICZBA_RODZAJOW]; // Sztuki na podajniku
    int sklep_otwarty; // 1 = otwarty, 0 = zamkniety
    int piekarnia_otwarta;
} DaneWspolne;



static int losowanie(int min, int max) {
    return min + (rand() % (max - min + 1));
}

#endif
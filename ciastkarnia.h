#ifndef CIASTKARNIA_H
#define CIASTKARNIA_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

#define LICZBA_RODZAJOW 12


static const char *NAZWA_PRODUKTOW[] = {
    "Pączek", "Sernik", "Szarlotka", "Drożdżówka", 
    "Kremówka", "Eklerek", "Mazurek", "Wuzetka", 
    "Rogalik", "Ptys", "Babeczka", "Tiramisu"
};

static const int CENY[] = {3, 15, 12, 4, 6, 5, 8, 7, 3, 5, 4, 10};


static int losowanie(int min, int max) {
    return min + (rand() % (max - min + 1));
}

#endif
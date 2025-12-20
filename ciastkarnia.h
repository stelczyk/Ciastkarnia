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
#include <sys/sem.h>

#define LICZBA_RODZAJOW 12
#define MAX_KLIENTOW 8
#define MAX_POJEMNOSC 20
#define PROG_DRUGIEJ KASY 4 
#define SCIEZKA_KLUCZA "/tmp"
#define PROJ_ID_SHM 'S'
#define PROJ_ID_SEM 'E'

struct sembuf;

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

static inline int semafor_zablokuj(int sem_id){
    struct sembuf operacja;
    operacja.sem_num = 0;
    operacja.sem_op = -1;
    operacja.sem_flg = 0;

    if(semop(sem_id, &operacja, 1) == -1){
        perror("blad semop zablokuj");
        return -1;
    }
    return 0;
}

static inline int semafor_odblokuj(int sem_id){
    struct sembuf operacja;
    operacja.sem_num = 0;
    operacja.sem_op = 1;
    operacja.sem_flg = 0;

    if(semop(sem_id, &operacja, 1) == -1){
        perror("blad semop odblokuj");
        return -1;
    }
    return 0;
}



#endif
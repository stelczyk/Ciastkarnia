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
#include <sys/msg.h>
#include <signal.h>
#include <stdarg.h>

//funkcja do logowania na ekran i do pliku
static void print_log(const char *format, ...) {
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);


    FILE *f = fopen("symulacja.txt", "a");
    if(f) {
        va_start(args, format);
        vfprintf(f, format, args);
        va_end(args);
        fclose(f);
    }
}




//KOLORY TERMINALA DLA CZYTELNOSCI

#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define CYAN        "\033[36m"
#define RED         "\033[31m"

#define KOLOR_KLIENT    GREEN
#define KOLOR_KASJER   CYAN
#define KOLOR_PIEKARZ   YELLOW
#define KOLOR_KIEROWNIK RED

//PARAMETRY

#define LICZBA_RODZAJOW 12 //ilosc rodzajow ciastek
#define MAX_KLIENTOW 16 // pojemnosc sklepu
#define PROG_DRUGIEJ_KASY (MAX_KLIENTOW/2) //otwieranie drugiej kasy
#define CZAS_PRZED_OTWARCIEM_SKLEPU 2 //czas pracy piekarni przed wejsciem klientow
#define CZAS_PRACY_SKLEPU 30 // czas pracy sklepu
#define MAX_NODES 2610

//KLUCZE IPC

#define SCIEZKA_KLUCZA "/tmp"
#define PROJ_ID_SHM 'S' //pamiec dzielona
#define PROJ_ID_MSG_KASA 'K' // kolejka kasy
#define PROJ_ID_SEM_GRUPA 'G' // grupa semaforowa

//SYGNALY

#define SYGNAL_INWENTARYZACJA SIGUSR1 //inwentaryzacja
#define SYGNAL_EWAKUACJA SIGUSR2 //ewakuacja

#define LICZBA_SEMAFOROW 10

#define SEM_MUTEX_DANE 0 //semafor do pamieci dzielonej
#define SEM_WEJSCIE_SKLEP 1 //ogranicza liczbe osob w sklepie
#define SEM_MAX_PROCESOW 2 //maksymalna liczba procesow
#define SEM_KOLEJKA_KASA1 3 //semafor kasy 1
#define SEM_KOLEJKA_KASA2 4 //semafor kasy 2
#define SEM_DZIALANIE_KASY1 5 //kasa 1 jest czynna? 1 = tak, 0 = nie
#define SEM_DZIALANIE_KASY2 6 //czy kasa2 jest czynna?
#define SEM_PODAJNIKI 7 //sekcja krytyczna piekarza
#define SEM_OUTPUT 8 //synchronizacja wypisywania
#define SEM_ILOSC_KLIENTOW 9

struct sembuf;

//NAZWY PRODUKTOW
static const char *NAZWA_PRODUKTOW[] = {
    "Paczek", "Sernik", "Szarlotka", "Drozdzowka", 
    "Kremowka", "Eklerek", "Mazurek", "Wuzetka", 
    "Rogalik", "Ptys", "Babeczka", "Tiramisu"
};

//CENY PRODUKTOW
static const int CENY[] = {3, 15, 12, 4, 6, 5, 8, 7, 3, 5, 4, 10};

//POJEMNOSC KAZDEGO PODAJNIKA
static const int POJEMNOSC_PODAJNIKA[] = {70, 45, 87, 73, 89, 90, 55, 86, 57, 45, 79, 94};

//PAMIEC DZIELONA
typedef struct {
    int type;           //rodzaj produktu (od 0 do LICZBA_RODZAJOW-1)
    int numer_sztuki;   //unikalny numer sztuki
    int next;           //indeks nastepnego elementu w tablicy node_pool
} ProductNode;

// PAMIEC DZIELONA
typedef struct {
    int wyprodukowano[LICZBA_RODZAJOW]; //statystyki calkowite
    int sprzedano[LICZBA_RODZAJOW]; // statystyki sprzedazy
    int na_podajniku[LICZBA_RODZAJOW]; //aktualny stan podajnikow
    int w_koszu[LICZBA_RODZAJOW]; //stan koszy przy ewakuacji

    int klienci_w_sklepie; //liczba klientow w sklepie
    int sklep_otwarty; // 1 = otwarty, 0 = zamkniety
    int piekarnia_otwarta; //1 = piekarz pracuje

    int inwentaryzacja; //flaga inwentaryzacji
    int ewakuacja; //flaga ewakuacji
    int zamykanie; //flaga zamykania

    int kasa_otwarta[2]; //czy dana kasa otwarta
    int kasa_kolejka[2]; //liczba klientow w kolejce
    int kasa_obsluzonych[2]; //liczba obsluzonych


    ProductNode node_pool[MAX_NODES];
    int free_head;
    int feeder_head[LICZBA_RODZAJOW];
    int feeder_tail[LICZBA_RODZAJOW];
} DaneWspolne;


//KOMUNIKAT KOSZYKA
typedef struct {
    long mtype; //typ, numer kasy
    pid_t klient_pid; //pid klienta
    int koszyk[LICZBA_RODZAJOW]; // zawartosc koszyka
    int suma; //suma do zaplaty
} MsgKoszyk;

//KOMINIKAT POTWIERDZENIA
typedef struct {
    long mtype;  // pid klienta
    int obsluzony; //1 == obsluzony
} MsgPotwierdzenie;

//funkcja losujaca liczbe
static int losowanie(int min, int max) {
    return min + (rand() % (max - min + 1));
}

static inline int semafor_zablokuj(int sem_id, int sem_num){
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = -1;
    operacja.sem_flg = SEM_UNDO;

    while(semop(sem_id, &operacja, 1) == -1){
        if(errno == EINTR) continue;  //retry po przerwaniu sygnalem
        perror("blad semop zablokuj");
        return -1;
    }
    return 0;
}

static inline int semafor_odblokuj(int sem_id, int sem_num){
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = 1;
    operacja.sem_flg = SEM_UNDO;

    while(semop(sem_id, &operacja, 1) == -1){
        if(errno == EINTR) continue;
        perror("blad semop odblokuj");
        return -1;
    }
    return 0;
}

static inline int semafor_zablokuj_bez_undo(int sem_id, int sem_num){
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = -1;
    operacja.sem_flg = 0;

    while(semop(sem_id, &operacja, 1) == -1){
        if(errno == EINTR) continue;
        perror("blad semop zablokuj_bez_undo");
        return -1;
    }
    return 0;
}

static inline int semafor_odblokuj_bez_undo(int sem_id, int sem_num){
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = 1;
    operacja.sem_flg = 0;

    while(semop(sem_id, &operacja, 1) == -1){
        if(errno == EINTR) continue;
        perror("blad semop odblokuj_bez_undo");
        return -1;
    }
    return 0;
}


static inline int semafor_wartosc(int sem_id, int sem_num){
    int val = semctl(sem_id, sem_num, GETVAL);
    if(val == -1){
        perror("Blad semctl do porbania wartosci semafora");
    }
    return val;
}

static inline int utworz_grupe_semaforowa(void){
    key_t klucz = ftok(SCIEZKA_KLUCZA, PROJ_ID_SEM_GRUPA);
    if(klucz == -1) return -1;
    return semget(klucz, LICZBA_SEMAFOROW, IPC_CREAT | 0600);
}

static inline int pobierz_grupe_semaforowa(void){
    key_t klucz = ftok(SCIEZKA_KLUCZA, PROJ_ID_SEM_GRUPA);
    if(klucz == -1) return -1;
    return semget(klucz, LICZBA_SEMAFOROW, 0600);
}

static inline int ustaw_wartosci_semaforow(int sem_id){
    if(semctl(sem_id, SEM_MUTEX_DANE, SETVAL, 1) == -1) return -1;
    if(semctl(sem_id, SEM_WEJSCIE_SKLEP, SETVAL, MAX_KLIENTOW) == -1) return -1;
    if(semctl(sem_id, SEM_MAX_PROCESOW, SETVAL,30 ) == -1) return -1;
    if(semctl(sem_id, SEM_KOLEJKA_KASA1, SETVAL, 1) == -1) return -1;
    if(semctl(sem_id, SEM_KOLEJKA_KASA2, SETVAL, 1) == -1) return -1;
    if(semctl(sem_id, SEM_DZIALANIE_KASY1, SETVAL, 1) == -1) return -1;
    if(semctl(sem_id, SEM_DZIALANIE_KASY2, SETVAL, 0) == -1) return -1;
    if(semctl(sem_id, SEM_PODAJNIKI, SETVAL, 1) == -1) return -1;
    if(semctl(sem_id, SEM_OUTPUT, SETVAL, 1) == -1) return -1;
    if(semctl(sem_id, SEM_ILOSC_KLIENTOW, SETVAL, 0) == -1) return -1;
    return 0;
}



#endif

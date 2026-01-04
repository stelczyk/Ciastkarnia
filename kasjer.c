#include "ciastkarnia.h"

static DaneWspolne *dane = NULL;
static int sem_id = -1;
static int msg_kasa_id = -1;

int main(int argc, char* argv[]){
    int numer_kasy = 1;
    if(argc > 1){
        numer_kasy = atoi(argv[1]);
    }

    pid_t kasjerID = getpid();
    printf("[KASJER %d] Kasa %d otwarta\n", kasjerID, numer_kasy);

    key_t klucz = ftok(SCIEZKA_KLUCZA, PROJ_ID_SHM);
    if(klucz == -1){
        perror("[KASJER] Blad ftok");
        exit(1);
    }

    int shm_id = shmget(klucz, sizeof(DaneWspolne), 0600);
    if(shm_id == -1){
        perror("[KASJER] Blad shmget");
        exit(1);
    }

    dane = (DaneWspolne *)shmat(shm_id, NULL, 0);
    if(dane == (void*)-1){
        perror("[KASJER] Blad shmat");
        exit(1);
    }

    key_t klucz_sem = ftok(SCIEZKA_KLUCZA, PROJ_ID_SEM);
    if(klucz_sem == -1){
        perror("[KASJER] Blad ftok sem");
        exit(1);
    }

    sem_id = semget(klucz_sem, 1, 0600);
    if(sem_id == -1){
        perror("[KASJER] Blad semget");
        exit(1);
    }

    key_t klucz_msg_kasa = ftok(SCIEZKA_KLUCZA, PROJ_ID_MSG_KASA);
    if(klucz_msg_kasa == -1){
        perror("[KASJER] Blad ftok msg kasa");
        exit(1);
    }

    msg_kasa_id = msgget(klucz_msg_kasa, 0600);
    if(msg_kasa_id == -1){
        perror("[KASJER] Blad msgget kasa");
        exit(1);
    }

    while(dane->sklep_otwarty){
        MsgKoszyk koszyk;

        ssize_t wynik = msgrcv(msg_kasa_id, &koszyk, sizeof(koszyk) - sizeof(long), numer_kasy, IPC_NOWAIT);
        if(wynik == -1){
            usleep(100000);
            continue;
        }
        printf("[KASJER %d] Kasa %d,  Obsluguje klienta %d\n", kasjerID, numer_kasy, koszyk.klient_pid);
        int pozycji = 0;
        for(int i = 0;i<LICZBA_RODZAJOW; i++){
            if(koszyk.koszyk[i] > 0){
                int wartosc = koszyk.koszyk[i] * CENY[i];
                printf("  %s x%d = %d zl\n", NAZWA_PRODUKTOW[i], koszyk.koszyk[i], wartosc);
                pozycji += koszyk.koszyk[i];
            }
        }

        printf("  SUMA: %d zl (%d szt.)\n", koszyk.suma, pozycji);
        printf("  Dziekujemy za zakupy!\n");

        usleep(500000);
    }
    printf("[KASJER %d] Kasa %d zamknieta\n", kasjerID, numer_kasy);
    shmdt(dane);
    return 0;
} 

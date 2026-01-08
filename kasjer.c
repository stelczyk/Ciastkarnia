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
    if(dane->kasa_otwarta[numer_kasy - 1]){
        printf("[KASJER %d] Kasa %d otwarta\n", kasjerID, numer_kasy);
    } else {
        printf("[KASJER %d] Kasa %d gotowa (czeka na otwarcie)\n", kasjerID, numer_kasy);
    }
    fflush(stdout);
    int poprzedni_status = dane->kasa_otwarta[numer_kasy - 1];

    while(dane->sklep_otwarty || dane->kasa_kolejka[numer_kasy - 1] > 0){
        if(dane->ewakuacja) {
            printf("[KASJER %d] Kasa %d - EWAKUACJA! Zamykam kasę!\n", kasjerID, numer_kasy);
            fflush(stdout);
            break;
        }

        int moja_kasa_index = numer_kasy - 1;

        int aktualny_status = dane->kasa_otwarta[moja_kasa_index];
        if(aktualny_status != poprzedni_status){
            if(aktualny_status){
                printf("[KASJER %d] Kasa %d, otwieram\n", kasjerID, numer_kasy);
                
            }else{
                printf("[KASJER %d] Kasa %d, zamykam, obsluguje reszte kolejki: %d\n", kasjerID, numer_kasy, dane->kasa_kolejka[moja_kasa_index]);  
            }
            fflush(stdout);
            poprzedni_status = aktualny_status;
        }    


        if(!dane->kasa_otwarta[moja_kasa_index]){
            if(dane->kasa_kolejka[moja_kasa_index] <= 0){
                usleep(200000);
                continue;
            }

        }
        MsgKoszyk koszyk;

        ssize_t wynik = msgrcv(msg_kasa_id, &koszyk, sizeof(koszyk) - sizeof(long), numer_kasy, IPC_NOWAIT);
        if(wynik == -1){
            usleep(100000);
            continue;
        }

        semafor_zablokuj(sem_id);
        if(dane->kasa_kolejka[moja_kasa_index] > 0){
            dane->kasa_kolejka[moja_kasa_index]--;
        }
        dane->kasa_obsluzonych[moja_kasa_index]++;
        semafor_odblokuj(sem_id);

        printf("[KASJER %d] Kasa %d,  Obsluguje klienta %d\n", kasjerID, numer_kasy, koszyk.klient_pid);
        fflush(stdout);
        int pozycji = 0;
        for(int i = 0;i<LICZBA_RODZAJOW; i++){
            if(koszyk.koszyk[i] > 0){
                int wartosc = koszyk.koszyk[i] * CENY[i];
                printf("  %s x%d = %d zl\n", NAZWA_PRODUKTOW[i], koszyk.koszyk[i], wartosc);
                pozycji += koszyk.koszyk[i];
            }
        }

        printf("  SUMA: %d zl (%d szt.) - Dziekujemy\n\n", koszyk.suma, pozycji);
        fflush(stdout);

        usleep(500000);
    }
    printf("[KASJER %d] Kasa %d zamknieta\n", kasjerID, numer_kasy);
    shmdt(dane);
    return 0;
} 

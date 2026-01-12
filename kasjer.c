#include "ciastkarnia.h"

static DaneWspolne *dane = NULL;
static int sem_id = -1;
static int msg_kasa_id = -1;

int main(int argc, char* argv[]){
    //odczytuje numer kasy z argumentu
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

    sem_id = pobierz_grupe_semaforowa();
    if(sem_id == -1){
        perror("[KASJER] Blad pobierania grupy semaforowej");
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
        printf(KOLOR_KASJER"[KASJER %d] Kasa %d otwarta\n"RESET, kasjerID, numer_kasy);
    } else {
        printf(KOLOR_KASJER"[KASJER %d] Kasa %d gotowa (czeka na otwarcie)\n"RESET, kasjerID, numer_kasy);
    }
    fflush(stdout);
    int poprzedni_status = dane->kasa_otwarta[numer_kasy - 1];

    while(dane->sklep_otwarty || dane->kasa_kolejka[numer_kasy - 1] > 0){
        if(dane->ewakuacja) {
            printf(KOLOR_KASJER"[KASJER %d] Kasa %d - EWAKUACJA! Zamykam kase!\n"RESET, kasjerID, numer_kasy);
            fflush(stdout);
            break;
        }

        int moja_kasa_index = numer_kasy - 1; //idneks w tablicy, 0 lub 1

        //sprawdz zmianę statusu kasy (otwarta/zamknięta)
        int aktualny_status = dane->kasa_otwarta[moja_kasa_index];
        if(aktualny_status != poprzedni_status){
            if(aktualny_status){
                printf(KOLOR_KASJER"[KASJER %d] Kasa %d, otwieram\n"RESET, kasjerID, numer_kasy);
                
            }else{
                printf(KOLOR_KASJER"[KASJER %d] Kasa %d, zamykam, obsluguje reszte kolejki: %d\n"RESET, kasjerID, numer_kasy, dane->kasa_kolejka[moja_kasa_index]);  
            }
            fflush(stdout);
            poprzedni_status = aktualny_status;
        }    

        //jesli kasa zamknięta i kolejka pusta, czekaj na otwarcie
        if(!dane->kasa_otwarta[moja_kasa_index]){
            if(dane->kasa_kolejka[moja_kasa_index] <= 0){
                int sem_dzialanie = (numer_kasy == 1) ? SEM_DZIALANIE_KASY1 : SEM_DZIALANIE_KASY2;
                semafor_zablokuj(sem_id, sem_dzialanie);
                continue;
            }

        }

        //odbiera koszyk od klienta
        MsgKoszyk koszyk;
        ssize_t wynik = msgrcv(msg_kasa_id, &koszyk, sizeof(koszyk) - sizeof(long), numer_kasy, 0);
        if(wynik == -1){
        
            if(errno == EINTR) continue;
            break; 
        }

        //Zaktualizuj liczniki kolejki
        int sem_kolejka = (numer_kasy == 1) ? SEM_KOLEJKA_KASA1 : SEM_KOLEJKA_KASA2;

        semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
        semafor_zablokuj(sem_id, sem_kolejka);
        if(dane->kasa_kolejka[moja_kasa_index] > 0){
            dane->kasa_kolejka[moja_kasa_index]--;
        }
        dane->kasa_obsluzonych[moja_kasa_index]++;
        semafor_odblokuj(sem_id, sem_kolejka);
        semafor_odblokuj(sem_id, SEM_MUTEX_DANE);

        //wydrukuj paragon
        semafor_zablokuj(sem_id, SEM_OUTPUT);

        printf(KOLOR_KASJER"[KASJER %d] Kasa %d, Obsluguje klienta %d\n"RESET, kasjerID, numer_kasy, koszyk.klient_pid);

        int pozycji = 0;
        for(int i = 0;i<LICZBA_RODZAJOW; i++){
            if(koszyk.koszyk[i] > 0){
                int wartosc = koszyk.koszyk[i] * CENY[i];
                printf(KOLOR_KASJER"  %s x%d = %d zl\n"RESET, NAZWA_PRODUKTOW[i], koszyk.koszyk[i], wartosc);
                pozycji += koszyk.koszyk[i];
            }
        }

        printf(KOLOR_KASJER"  SUMA: %d zl (%d szt.) - Dziekujemy\n\n"RESET, koszyk.suma, pozycji);
        fflush(stdout);
        semafor_odblokuj(sem_id, SEM_OUTPUT);
        //usleep(800000);

        // Wyślij potwierdzenie do klienta
        MsgPotwierdzenie potwierdzenie;
        potwierdzenie.mtype = koszyk.klient_pid;
        potwierdzenie.obsluzony = 1;
        msgsnd(msg_kasa_id, &potwierdzenie, sizeof(potwierdzenie) - sizeof(long), 0);

        }
    printf(KOLOR_KASJER"[KASJER %d] Kasa %d zamknieta\n"RESET, kasjerID, numer_kasy);
    shmdt(dane);
    return 0;
} 
#include "ciastkarnia.h"

static int sem_id = -1;

static DaneWspolne *dane = NULL;

int main(){
    srand(time(NULL));

    pid_t piekarzID = getpid();

    printf("[PIEKARZ %d] Rozpoczynam prace\n", piekarzID);

    key_t klucz = ftok(SCIEZKA_KLUCZA, PROJ_ID_SHM);
    if(klucz == -1){
        perror("[PIEKARZ] Blad ftok");
        exit(1);
    }

    int shm_id = shmget(klucz, sizeof(DaneWspolne), 0600);
    if(shm_id == -1){
        perror("[PIEKARZ] Blad shmget");
        exit(1);
    }

    dane = (DaneWspolne *)shmat(shm_id, NULL, 0);
    if(dane == (void*)-1){
        perror("[PIEKARZ] Blad shmat");
        exit(1);
    }

    printf("[PIEKARZ %d] Polaczylem sie z pamiecia dzielona\n", piekarzID);

    key_t klucz_sem = ftok(SCIEZKA_KLUCZA, PROJ_ID_SEM);
    if(klucz_sem == -1){
        perror("[PIEKARZ] Blad ftok semafor");
        exit(1);
    }

    sem_id = semget(klucz_sem, 1, 0600);
    if(sem_id == -1){
        perror("[PIKERAZ] Blad semget");
        exit(1);
    }

    printf("[Piekarz] Polaczono z semaforem\n");

    while(dane->piekarnia_otwarta){
        int indeks = losowanie(0,LICZBA_RODZAJOW - 1);
        int czasPieczenia = losowanie(1,3);
        int ilosc = losowanie(2,10);

        printf("[PIEKARZ %d] Rozpoczynam pieczenie %s\n", piekarzID, NAZWA_PRODUKTOW[indeks]);
        sleep(czasPieczenia);

        semafor_zablokuj(sem_id);

        int aktualnie = dane->podajnik[indeks];
        int wolne = MAX_POJEMNOSC - aktualnie;
        
        //sprawdza pojemnosc
        if(wolne <= 0){
            printf("[PIEKARZ %d] Podajnik %s pelny!\n", piekarzID, NAZWA_PRODUKTOW[indeks]);

            semafor_odblokuj(sem_id);

            sleep(2);
            continue;
        }
        //dodaje tylko tyle ile sie miesci
        if(ilosc > wolne){
            ilosc = wolne;
        }

        dane->podajnik[indeks] += ilosc;

        printf("[PIEKARZ %d] Upieklem %d sztuk %s  (na podajniku: %d/%d)\n", piekarzID, ilosc, NAZWA_PRODUKTOW[indeks], dane->podajnik[indeks], MAX_POJEMNOSC);

        semafor_odblokuj(sem_id);

        usleep(500000);

        printf("[PIEKARZ %d] Wykladam %s na podajnik\n", piekarzID,NAZWA_PRODUKTOW[indeks]);

        sleep(1);
    }

    printf("[PIEKARZ %d] Piekarnia zamknieta, koncze prace\n", piekarzID);
    shmdt(dane);
    return 0;
}
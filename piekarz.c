#include "ciastkarnia.h"



static int sem_id = -1;
static DaneWspolne *dane = NULL;
static int msg_id = -1;
static int licznik_produktow = 0;

int main(){
    srand(time(NULL));

    pid_t piekarzID = getpid();

    printf( KOLOR_PIEKARZ"[PIEKARZ %d] Rozpoczynam prace\n" RESET, piekarzID);
    fflush(stdout);

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

    //printf("[PIEKARZ %d] Polaczylem sie z pamiecia dzielona\n", piekarzID);

   sem_id = pobierz_grupe_semaforowa();
   if(sem_id == -1){
        perror("[PIEKARZ] Blad pobierania grupy semaforowej");
        exit(1);
   }

   

    //printf("[Piekarz] Polaczono z semaforem\n");

    key_t klucz_msg = ftok(SCIEZKA_KLUCZA, PROJ_ID_MSG);
    if (klucz_msg == -1){
        perror("[PIEKARZ] Blad ftok dla kolejki");
        exit(1);
    }

    msg_id = msgget(klucz_msg, 0600);
    if(msg_id == -1){
        perror("[PIEKARz] Blad msgget");
        exit(1);
    }
    //printf("[PIEKARZ %d] Polaczylem sie z kolejka komunikatow\n");

    while(dane->piekarnia_otwarta){ //sprawdza czy nie ma ewakuacji
        if(dane->ewakuacja) {
            printf(KOLOR_PIEKARZ"[PIEKARZ %d] EWAKUACJA! Koncze prace!\n"RESET, piekarzID);
            fflush(stdout);
            break;
        }
        //losuje rodzaj i ilosc
        int indeks = losowanie(0,LICZBA_RODZAJOW - 1);
        int czasPieczenia = losowanie(1,3);
        int ilosc = losowanie(2,10);

        printf(KOLOR_PIEKARZ"[PIEKARZ %d] Pieke %s (%d sztuk)\n"RESET, piekarzID, NAZWA_PRODUKTOW[indeks], ilosc);
        fflush(stdout);
        
        //piecze kazda sztuke
        for (int i = 0; i<ilosc; i++){


            //sprawdz czy podajnik nie jest pelny
            semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
            semafor_zablokuj(sem_id, SEM_PODAJNIKI);

            int aktualnie = dane->na_podajniku[indeks];
            int limit = POJEMNOSC_PODAJNIKA[indeks];

            if(aktualnie >= limit){
                semafor_odblokuj(sem_id, SEM_PODAJNIKI);
                semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
                printf(KOLOR_PIEKARZ"[PIEKARZ %d] Podajnik %s pelny (%d/%d). STOP\n"RESET, piekarzID, NAZWA_PRODUKTOW[indeks], aktualnie,limit);
                fflush(stdout);
                break;
            }
            semafor_odblokuj(sem_id, SEM_PODAJNIKI);
            semafor_odblokuj(sem_id, SEM_MUTEX_DANE);


            //przygotuj produkt do wyslania
            if(semafor_zablokuj(sem_id, SEM_MUTEX_DANE) == -1) { perror("[PIEKARZ] sem"); break; }
            if(semafor_zablokuj(sem_id, SEM_PODAJNIKI) == -1) { semafor_odblokuj(sem_id, SEM_MUTEX_DANE); perror("[PIEKARZ] sem"); break; }

            licznik_produktow++;
            MsgProdukt produkt;
            produkt.mtype = indeks + 1;
            produkt.numer_sztuki = licznik_produktow;

            if (msgsnd(msg_id, &produkt, sizeof(produkt) - sizeof(long), IPC_NOWAIT) == -1) {
                if(errno == EAGAIN) {
                    licznik_produktow--;
                    semafor_odblokuj(sem_id, SEM_PODAJNIKI);
                    semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
                    //usleep(100000);
                    continue;
                } else {
                    perror("[PIEKARZ] Blad msgsnd");
                    semafor_odblokuj(sem_id, SEM_PODAJNIKI);
                    semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
                    continue;
                }
            }

            //aktualizuj statystyki
            dane->wyprodukowano[indeks]++;
            dane->na_podajniku[indeks]++;
            int ile_teraz = dane->na_podajniku[indeks];

            semafor_odblokuj(sem_id, SEM_PODAJNIKI);
            semafor_odblokuj(sem_id, SEM_MUTEX_DANE);

            semafor_zablokuj(sem_id, SEM_OUTPUT);
            printf(KOLOR_PIEKARZ"[PIEKARZ %d] Wyslalem %s # %d na podajnik (%d/%d)\n"RESET,
                   piekarzID, NAZWA_PRODUKTOW[indeks], licznik_produktow, ile_teraz, POJEMNOSC_PODAJNIKA[indeks]);
            fflush(stdout);
            semafor_odblokuj(sem_id, SEM_OUTPUT);
        }
        //usleep(100000); //xd
        
    }

    printf(KOLOR_PIEKARZ"[PIEKARZ %d] Piekarnia zamknieta, koncze prace\n"RESET, piekarzID);
    fflush(stdout);
    shmdt(dane);
    return 0;
}
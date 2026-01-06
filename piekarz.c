#include "ciastkarnia.h"



static int sem_id = -1;
static DaneWspolne *dane = NULL;
static int msg_id = -1;
static int licznik_produktow = 0;

int main(){
    srand(time(NULL));

    pid_t piekarzID = getpid();

    printf("[PIEKARZ %d] Rozpoczynam prace\n", piekarzID);
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

    while(dane->piekarnia_otwarta){
        int indeks = losowanie(0,LICZBA_RODZAJOW - 1);
        int czasPieczenia = losowanie(1,3);
        int ilosc = losowanie(2,10);

        printf("[PIEKARZ %d] Pieke %s (%d sztuk)\n", piekarzID, NAZWA_PRODUKTOW[indeks], ilosc);
        fflush(stdout);

        for (int i = 0; i<ilosc; i++){

            semafor_zablokuj(sem_id);
            int aktualnie = dane->na_podajniku[indeks];
            int limit = POJEMNOSC_PODAJNIKA[indeks];

            if(aktualnie >= limit){
                semafor_odblokuj(sem_id);
                printf("[PIEKARZ %d] Podajnik %s pelny (%d/%d). STOP\n", piekarzID, NAZWA_PRODUKTOW[indeks], aktualnie,limit);
                fflush(stdout);
                break;
            }
            semafor_odblokuj(sem_id);


            licznik_produktow++;
            MsgProdukt produkt;
            produkt.mtype = indeks +1;
            produkt.numer_sztuki = licznik_produktow;

            if (msgsnd(msg_id, &produkt, sizeof(produkt) - sizeof(long), 0)== -1){
                perror("[PIEKARZ] Blad msgsnd");
            }else {
                semafor_zablokuj(sem_id);
                dane->wyprodukowano[indeks]++;
                dane->na_podajniku[indeks]++;
                int ile_teraz = dane->na_podajniku[indeks];
                semafor_odblokuj(sem_id);
                printf("[PIEKARZ %d] Wyslalem %s # %d na podajnik (%d/%d)\n", piekarzID, NAZWA_PRODUKTOW[indeks], licznik_produktow, ile_teraz, POJEMNOSC_PODAJNIKA[indeks]);
                fflush(stdout);
            }
        }
        sleep(1);
    }

    printf("[PIEKARZ %d] Piekarnia zamknieta, koncze prace\n", piekarzID);
    fflush(stdout);
    shmdt(dane);
    return 0;
}
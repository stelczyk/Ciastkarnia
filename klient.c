#include "ciastkarnia.h"

static DaneWspolne *dane = NULL;
static int sem_id = -1;

int main(){
    srand(time(NULL));
    pid_t klientID = getpid();

    printf("[KLIENT %d] Wchodze do ciastkarni\n", klientID);

    key_t klucz = ftok(SCIEZKA_KLUCZA, PROJ_ID_SHM);
    if(klucz == -1){
        perror("[KLIENT] Blad ftok");
        exit(1);
    }

    int shm_id = shmget(klucz, sizeof(DaneWspolne), 0600);
    if(shm_id == -1){
        perror("[KLIENT] Blad shmget");
        exit(1);
    }

    dane = (DaneWspolne *)shmat(shm_id, NULL, 0);
    if (dane == (void*)-1){
        perror("[KLIENT] Blad shmat");
        exit(1);
    }

    printf("[KLIENT %d] Polaczona z pamiecia dzielona\n", klientID);
    sleep(1);

    key_t klucz_sem = ftok(SCIEZKA_KLUCZA, PROJ_ID_SEM);
    if(klucz_sem == -1){
        perror("[KLIENT] Blad ftok dla semafora");
        exit(1);
    }
    sem_id = semget(klucz_sem, 1,0600);
    if(sem_id == -1){
        perror("[KLIENT] Blad semget");
        exit(1);
    }

    printf("[KLIENT %d] Polaczono z semaforem\n", klientID);

    int ileProduktow = losowanie(2,4);
    int listaZakupow[LICZBA_RODZAJOW];

    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        listaZakupow[i] = 0;
    }

    int wybrano = 0;
    while(wybrano < ileProduktow){
        int indeks = losowanie(0, LICZBA_RODZAJOW-1);
        if(listaZakupow[indeks] == 0){
            listaZakupow[indeks] = losowanie(1,3);
            wybrano++;
        } 
    }

    printf("[KLIENT %d] Chce kupic: \n", klientID);
    for(int i = 0; i<LICZBA_RODZAJOW;i++){
        if(listaZakupow[i] > 0){
            printf("  - %s: %d szt.\n", NAZWA_PRODUKTOW[i], listaZakupow[i]);
        }
    }

    int koszyk[LICZBA_RODZAJOW];
    int suma = 0;

    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        koszyk[i] = 0;
    }

    printf("[KLIENT %d] Robie zakupy\n", klientID);

    for (int i = 0; i< LICZBA_RODZAJOW; i++){
        if(listaZakupow[i] == 0)continue;

        semafor_zablokuj(sem_id);

        int chce = listaZakupow[i];
        int dostepne = dane->podajnik[i];

        if(dostepne == 0){
            printf("[KLIENT %d] Brak %s na podajniku\n", klientID, NAZWA_PRODUKTOW[i]);
            semafor_odblokuj(sem_id);
            continue;
        }

        int biore = (chce < dostepne) ? chce : dostepne;

        dane->podajnik[i] -= biore;
        koszyk[i] = biore;
        suma += biore * CENY[i];

         printf("[KLIENT %d] Biore %d szt. %s (zostalo: %d)\n", 
               klientID, biore, NAZWA_PRODUKTOW[i], dane->podajnik[i]);

        semafor_odblokuj(sem_id);

        usleep(200000); 
    }


    sleep(1);

    printf("\n[KLIENT %d] === RACHUNEK ===\n", klientID);
    int kupiono = 0;
    for(int i = 0;i < LICZBA_RODZAJOW; i++){
        if(koszyk[i] > 0){
            int wartosc = koszyk[i] * CENY[i];
            printf("  %s: %d szt. x %d zl = %d zl\n", 
                   NAZWA_PRODUKTOW[i], koszyk[i], CENY[i], wartosc);
            kupiono += koszyk[i];
        }
    }
    printf("  -------------------------\n");
    printf("  SUMA: %d zl za %d produktow\n", suma, kupiono);
    printf("[KLIENT %d] =================\n\n", klientID);
    
    shmdt(dane);
    printf("[KLIENT %d] Wychodzę\n", klientID);

    return 0;
}
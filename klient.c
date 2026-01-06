#include "ciastkarnia.h"

static DaneWspolne *dane = NULL;
static int sem_id = -1;
static int msg_id = -1;
static int sem_wejscie_id = -1;
static int msg_kasa_id = -1; 

int main(){
    srand(time(NULL));
    pid_t klientID = getpid();

    
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
    if(dane == (void*)-1){
        perror("[KLIENT] Blad shmat");
        exit(1);
    }

    
    key_t klucz_sem = ftok(SCIEZKA_KLUCZA, PROJ_ID_SEM);
    if(klucz_sem == -1){
        perror("[KLIENT] Blad ftok dla semafora");
        exit(1);
    }

    sem_id = semget(klucz_sem, 1, 0600);
    if(sem_id == -1){
        perror("[KLIENT] Blad semget");
        exit(1);
    }

    
    key_t klucz_msg = ftok(SCIEZKA_KLUCZA, PROJ_ID_MSG);
    if(klucz_msg == -1){
        perror("[KLIENT] Blad ftok dla kolejki");
        exit(1);
    }

    msg_id = msgget(klucz_msg, 0600);
    if(msg_id == -1){
        perror("[KLIENT] Blad msgget");
        exit(1);
    }

   
    key_t klucz_sem_wejscie = ftok(SCIEZKA_KLUCZA, PROJ_ID_SEM_WEJSCIE);
    if(klucz_sem_wejscie == -1){
        perror("[KLIENT] Blad ftok dla semafora wejscia");
        exit(1);
    }

    sem_wejscie_id = semget(klucz_sem_wejscie, 1, 0600);
    if(sem_wejscie_id == -1){
        perror("[KLIENT] Blad semget dla semafora wejscia");
        exit(1);
    }

    key_t klucz_msg_kasa = ftok(SCIEZKA_KLUCZA, PROJ_ID_MSG_KASA);
    if(klucz_msg_kasa == -1){
        perror("[KLIENT] Blad ftok dla kolejki kas");
        exit(1);
    }

    msg_kasa_id = msgget(klucz_msg_kasa, 0600);
    if(msg_kasa_id == -1){
        perror("[KLIENT] Blad mssget dla kas");
        exit(1);
    }

    
    int wolne_miejsca = semafor_wartosc(sem_wejscie_id);
    if(wolne_miejsca <= 0){
        printf("[KLIENT %d] Sklep pelny, czekam przed sklepem...\n", klientID);
        fflush(stdout);
    }

    semafor_zablokuj(sem_wejscie_id);

    semafor_zablokuj(sem_id);

    dane->klienci_w_sklepie++;
    int ilu_w_sklepie = dane->klienci_w_sklepie;
    printf("[KLIENT %d] Wchodze do sklepu (klientow: %d/%d)\n", klientID, ilu_w_sklepie, MAX_KLIENTOW);

    if(ilu_w_sklepie >= PROG_DRUGIEJ_KASY && !dane->kasa_otwarta[1]){
        dane->kasa_otwarta[1] = 1;
        printf("[SYSTEM] Otwieram kase 2,   Klientow: %d >= %d\n", ilu_w_sklepie, PROG_DRUGIEJ_KASY);
        fflush(stdout);
    }
           
    semafor_odblokuj(sem_id);

    //printf("[KLIENT %d] Wchodze do sklepu (klientow: %d/%d)\n", klientID, ilu_w_sklepie, MAX_KLIENTOW);
           

    sleep(3);

    
    int ileProduktow = losowanie(2, 4);
    int listaZakupow[LICZBA_RODZAJOW];

    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        listaZakupow[i] = 0;
    }

    int wybrano = 0;
    while(wybrano < ileProduktow){
        int indeks = losowanie(0, LICZBA_RODZAJOW - 1);
        if(listaZakupow[indeks] == 0){
            listaZakupow[indeks] = losowanie(1, 3);
            wybrano++;
        }
    }

    printf("[KLIENT %d] Chce kupic:\n", klientID);
    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        if(listaZakupow[i] > 0){
            printf("  - %s: %d szt.", NAZWA_PRODUKTOW[i], listaZakupow[i]);
        }
    }
    printf("\n");
    fflush(stdout);

  
    int koszyk[LICZBA_RODZAJOW];
    int suma = 0;

    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        koszyk[i] = 0;
    }

    printf("[KLIENT %d] Robie zakupy\n", klientID);
    fflush(stdout);

    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        if(listaZakupow[i] == 0) continue;

        int chce = listaZakupow[i];
        int wzialem = 0;

        for(int j = 0; j < chce; j++){
            MsgProdukt produkt;

            ssize_t wynik = msgrcv(msg_id, &produkt, sizeof(produkt) - sizeof(long), i + 1, IPC_NOWAIT);
            if(wynik == -1){
                break;
            }

            wzialem++;
            koszyk[i]++;
            suma += CENY[i];

            semafor_zablokuj(sem_id);
            dane->sprzedano[i]++;
            dane->na_podajniku[i]--;
            semafor_odblokuj(sem_id);

            printf("[KLIENT %d] Wzialem %s #%d\n", klientID, NAZWA_PRODUKTOW[i], produkt.numer_sztuki);
            fflush(stdout);
                   
        }

        if(wzialem == 0){
            printf("[KLIENT %d] Brak %s na podajniku!\n", klientID, NAZWA_PRODUKTOW[i]);
            fflush(stdout);
                   
        } else if(wzialem < chce){
            printf("[KLIENT %d] Wzialem tylko %d/%d szt. %s\n", klientID, wzialem, chce, NAZWA_PRODUKTOW[i]);
            fflush(stdout);
                   
        }

        usleep(100000);
    }

    
    if(suma > 0){
    
    semafor_zablokuj(sem_id);

    int wybrana_kasa = 1;
    if(dane->kasa_otwarta[1]){
        if(dane->kasa_kolejka[1] < dane->kasa_kolejka[0]){
            wybrana_kasa = 2;
        }
    }

    dane->kasa_kolejka[wybrana_kasa - 1]++;
    int moja_pozycja = dane->kasa_kolejka[wybrana_kasa - 1];
    semafor_odblokuj(sem_id);

    printf("[KLIENT %d] Ide do kasy %d, pozycja w kolecje %d\n", klientID, wybrana_kasa, moja_pozycja);
    fflush(stdout);
    sleep(3);

    MsgKoszyk wiadomosc;
    wiadomosc.mtype = wybrana_kasa;
    wiadomosc.klient_pid = klientID;
    wiadomosc.suma = suma;
    for(int i = 0;i < LICZBA_RODZAJOW; i++){
        wiadomosc.koszyk[i] = koszyk[i];
    }

    if(msgsnd(msg_kasa_id, &wiadomosc, sizeof(wiadomosc) - sizeof(long),0) == -1){
        perror("[KLIENT] Blad msgsnd dla kasy");
    }
    sleep(1);
    }else{
        printf("[KLIENT %d] Nic nie kupilem, wychodze\n", klientID);
        fflush(stdout);
    }
    
    semafor_zablokuj(sem_id);
    dane->klienci_w_sklepie--;
    int zostalo = dane->klienci_w_sklepie;
    printf("[KLIENT %d] Wychodzę (zostalo klientow: %d)\n", klientID, zostalo);

    if(zostalo < PROG_DRUGIEJ_KASY && dane->kasa_otwarta[1]){
        dane->kasa_otwarta[1] = 0;
        printf("[SYSTEM] Zamykam kase 2,   Klientow: %d < %d\n", zostalo, PROG_DRUGIEJ_KASY);
        fflush(stdout);
    }
    semafor_odblokuj(sem_id);

    semafor_odblokuj(sem_wejscie_id);

    
   

    shmdt(dane);
    return 0;
}
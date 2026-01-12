#include "ciastkarnia.h"

static DaneWspolne *dane = NULL;
static int sem_id = -1;
static int msg_id = -1;
//static int sem_wejscie_id = -1;
static int msg_kasa_id = -1; 

int main(){ 
    srand(time(NULL) + getpid());
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

    
    sem_id = pobierz_grupe_semaforowa();
    if(sem_id == -1){
        perror("[KLIENT] Blad pobierania grupy semaforowej");
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

    //sprawdz czy sklep jest pelny
    semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
    int w_sklepie = dane->klienci_w_sklepie;
    semafor_odblokuj(sem_id, SEM_MUTEX_DANE);

    if(w_sklepie >= MAX_KLIENTOW) {
        printf(KOLOR_KLIENT"[KLIENT %d] Sklep pelny (%d/%d), czekam przed sklepem...\n"RESET, klientID, w_sklepie, MAX_KLIENTOW);
    fflush(stdout);
}

    //zablokuj semafor wejścia
    semafor_zablokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);

    //sprawdz czy sklep nadal otwarty
    semafor_zablokuj(sem_id, SEM_MUTEX_DANE);


    if(!dane->sklep_otwarty || dane->zamykanie){
        semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
        semafor_odblokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);
        semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);
        printf( KOLOR_KLIENT"[KLIENT %d] Sklep zamkniety, nie wchodze.\n" RESET, klientID);
        fflush(stdout);
        shmdt(dane);
        exit(0);
    }

    //wejdz do sklepu
    dane->klienci_w_sklepie++;
    int ilu_w_sklepie = dane->klienci_w_sklepie;

    semafor_zablokuj(sem_id, SEM_OUTPUT);
    printf( KOLOR_KLIENT "[KLIENT %d] Wchodze do sklepu (klientow: %d/%d)\n"RESET, klientID, ilu_w_sklepie, MAX_KLIENTOW);
    fflush(stdout);
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    //otworz drugą kase jesli potrzeba
    if(ilu_w_sklepie >= PROG_DRUGIEJ_KASY && !dane->kasa_otwarta[1]){
        dane->kasa_otwarta[1] = 1;
        semafor_odblokuj(sem_id, SEM_DZIALANIE_KASY2);
        printf("[SYSTEM] Otwieram kase 2,   Klientow: %d >= %d\n", ilu_w_sklepie, PROG_DRUGIEJ_KASY);
        fflush(stdout);
    }
       
    semafor_odblokuj(sem_id, SEM_MUTEX_DANE);


    
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

    //Wyswietl liste zakupow
    printf(KOLOR_KLIENT "[KLIENT %d] Chce kupic:\n"RESET, klientID);
    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        if(listaZakupow[i] > 0){
            printf(KOLOR_KLIENT"  - %s: %d szt."RESET, NAZWA_PRODUKTOW[i], listaZakupow[i]);
        }
    }
    printf("\n");
    fflush(stdout);

  
    int koszyk[LICZBA_RODZAJOW];
    int suma = 0;

    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        koszyk[i] = 0;
    }

    printf(KOLOR_KLIENT "[KLIENT %d] Robie zakupy\n"RESET, klientID);
    fflush(stdout);
    usleep(200000);
    
    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        //sprawdz ewakuację
        if(dane->ewakuacja) {
            semafor_zablokuj(sem_id, SEM_OUTPUT);
            printf(KOLOR_KLIENT "[KLIENT %d] EWAKUACJA! Odkładam towar do kosza:\n"RESET, klientID);
    
            int cos_mialem = 0;
            for(int j = 0; j < LICZBA_RODZAJOW; j++) {
                if(koszyk[j] > 0) {
                    printf(KOLOR_KLIENT "  - %s: %d szt.\n"RESET, NAZWA_PRODUKTOW[j], koszyk[j]);
                    cos_mialem = 1;
                }
            }
            if(!cos_mialem) {
                printf(KOLOR_KLIENT "  (koszyk byl pusty - nic nie wzialem)\n"RESET);
            }
            fflush(stdout);
            semafor_odblokuj(sem_id, SEM_OUTPUT);
    
            semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
            for(int j = 0; j < LICZBA_RODZAJOW; j++) {
                dane->w_koszu[j] += koszyk[j];
                koszyk[j] = 0;
            }
        dane->klienci_w_sklepie--;
        semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
        semafor_odblokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);
        semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);
        shmdt(dane);
        exit(0);
    }
    
    //pomin produkty ktorych nie chcemy
    if(listaZakupow[i] == 0) continue;

        int chce = listaZakupow[i];
        int wzialem = 0;

        //probuj pobrac kazda sztuke
        for(int j = 0; j < chce; j++){
            MsgProdukt produkt;

            ssize_t wynik = msgrcv(msg_id, &produkt, sizeof(produkt) - sizeof(long), i + 1, IPC_NOWAIT);
            if(wynik == -1){
                break;
            }

            wzialem++;
            koszyk[i]++;
            suma += CENY[i];
            
            //aktualizuj statystyki
            semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
            dane->sprzedano[i]++;
            dane->na_podajniku[i]--;
            semafor_odblokuj(sem_id, SEM_MUTEX_DANE);

            semafor_zablokuj(sem_id, SEM_OUTPUT);
            printf(KOLOR_KLIENT "[KLIENT %d] Wzialem %s #%d\n"RESET, klientID, NAZWA_PRODUKTOW[i], produkt.numer_sztuki);
            fflush(stdout);
            semafor_odblokuj(sem_id, SEM_OUTPUT);
            usleep(200000);
        }

        //komunikat o braku towaru
        if(wzialem == 0){
            semafor_zablokuj(sem_id, SEM_OUTPUT);
            printf(KOLOR_KLIENT "[KLIENT %d] Brak %s na podajniku!\n"RESET, klientID, NAZWA_PRODUKTOW[i]);
            fflush(stdout);
            semafor_odblokuj(sem_id, SEM_OUTPUT);
                   
        } else if(wzialem < chce){
            printf(KOLOR_KLIENT "[KLIENT %d] Wzialem tylko %d/%d szt. %s\n"RESET, klientID, wzialem, chce, NAZWA_PRODUKTOW[i]);
            fflush(stdout);        
        }
        
    }

    //sprawdz ewakuacje po zakończeniu zakupów
    if(dane->ewakuacja) {
        semafor_zablokuj(sem_id, SEM_OUTPUT);
        printf(KOLOR_KLIENT "[KLIENT %d] EWAKUACJA! Odkładam towar do kosza:\n"RESET, klientID);
    
        int cos_mialem = 0;
        for(int j = 0; j < LICZBA_RODZAJOW; j++) {
            if(koszyk[j] > 0) {
                printf(KOLOR_KLIENT "  - %s: %d szt.\n"RESET, NAZWA_PRODUKTOW[j], koszyk[j]);
                cos_mialem = 1;
            }
        }
        if(!cos_mialem) {
                printf(KOLOR_KLIENT "  (koszyk byl pusty - nic nie wzialem)\n"RESET);
        }
        fflush(stdout);
        semafor_odblokuj(sem_id, SEM_OUTPUT);

        semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
        for(int j = 0; j < LICZBA_RODZAJOW; j++) {
            dane->w_koszu[j] += koszyk[j];
        }
        dane->klienci_w_sklepie--;
        semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
        semafor_odblokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);
        semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);
        shmdt(dane);
        exit(0);
    }

    //podsumowanie zakupow
    semafor_zablokuj(sem_id, SEM_OUTPUT);
    printf(KOLOR_KLIENT "[KLIENT %d] Skonczylem zakupy, w koszyku mam:\n"RESET, klientID);
    int ile_mam = 0;
    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        if(koszyk[i] > 0){
            printf(KOLOR_KLIENT "  - %s: %d szt.\n"RESET, NAZWA_PRODUKTOW[i], koszyk[i]);
            ile_mam += koszyk[i];
        }
    }
    if(ile_mam == 0){
        printf(KOLOR_KLIENT "  (nic - wszystko bylo niedostepne)\n"RESET);
    }   
    printf(KOLOR_KLIENT "  SUMA: %d zl\n"RESET, suma);
    fflush(stdout);
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    //obsluga przy kasie
    if(suma > 0){
        
        semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
        semafor_zablokuj(sem_id, SEM_KOLEJKA_KASA1);
        semafor_zablokuj(sem_id, SEM_KOLEJKA_KASA2);

        //sprawdz dostepnosc kas
        int kasa1_dziala = (kill(dane->kasjer1_pid, 0) == 0);
        int kasa2_zyje = (kill(dane->kasjer2_pid, 0) == 0);

        //awaryjne otwarcie kasy 2 gdy kasa 1 nie dziala
        if (!kasa1_dziala && kasa2_zyje && !dane->kasa_otwarta[1]) {
            dane->kasa_otwarta[1] = 1;
            semafor_odblokuj(sem_id, SEM_DZIALANIE_KASY2);
            printf("[SYSTEM] Awaryjne otwarcie kasy 2 - kasa 1 nie dziala!\n");
            fflush(stdout);
        }

        int kasa2_dziala = (dane->kasa_otwarta[1] && kasa2_zyje);

        int wybrana_kasa;
        if(kasa1_dziala && kasa2_dziala){
            if(dane->kasa_kolejka[1] < dane->kasa_kolejka[0]){
                wybrana_kasa = 2;
            }else{
                wybrana_kasa = 1;
            }
        } else if (kasa1_dziala){
            wybrana_kasa = 1;
        }else if(kasa2_dziala){
            wybrana_kasa = 2;
        } else {
            printf(KOLOR_KLIENT "[KLIENT %d] zadna kasa nie dziala. Zostawiam towar i wychodze\n"RESET, klientID);
            fflush(stdout);
            for(int j = 0; j<LICZBA_RODZAJOW; j++){
                dane->w_koszu[j] += koszyk[j];
            }
            dane->klienci_w_sklepie--;

            semafor_odblokuj(sem_id, SEM_KOLEJKA_KASA2);
            semafor_odblokuj(sem_id, SEM_KOLEJKA_KASA1);
            semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
            semafor_odblokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);
            semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);
            shmdt(dane);
            exit(0);
        }

        //dołacz do kolejki
        dane->kasa_kolejka[wybrana_kasa - 1]++;
        int moja_pozycja = dane->kasa_kolejka[wybrana_kasa - 1];

        semafor_odblokuj(sem_id, SEM_KOLEJKA_KASA2);
        semafor_odblokuj(sem_id, SEM_KOLEJKA_KASA1);
        semafor_odblokuj(sem_id, SEM_MUTEX_DANE);

        semafor_zablokuj(sem_id, SEM_OUTPUT);
        printf(KOLOR_KLIENT "[KLIENT %d] Ide do kasy %d, pozycja w kolecje %d\n"RESET, klientID, wybrana_kasa, moja_pozycja);
        fflush(stdout);
        semafor_odblokuj(sem_id, SEM_OUTPUT);
        
        //wyslij koszyk do kasy
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

        // Czekaj na potwierdzenie od kasjera
        MsgPotwierdzenie potwierdzenie;
        msgrcv(msg_kasa_id, &potwierdzenie, sizeof(potwierdzenie) - sizeof(long), klientID, 0);
    }else{
        printf(KOLOR_KLIENT "[KLIENT %d] Nic nie kupilem, wychodze\n"RESET, klientID);
        fflush(stdout);
    }
    
    //wyjscie ze sklepu
    semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
    dane->klienci_w_sklepie--;
    int zostalo = dane->klienci_w_sklepie;

    semafor_zablokuj(sem_id, SEM_OUTPUT);
    printf(KOLOR_KLIENT "[KLIENT %d] Wychodze (zostalo klientow: %d)\n"RESET, klientID, zostalo);
    fflush(stdout);
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    //zamknij kase 2 jesli malo klientww
    if(zostalo < PROG_DRUGIEJ_KASY && dane->kasa_otwarta[1]){
        dane->kasa_otwarta[1] = 0;
        printf("[SYSTEM] Zamykam kase 2,   Klientow: %d < %d\n", zostalo, PROG_DRUGIEJ_KASY);
        fflush(stdout);
    }

    semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
    semafor_odblokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);
    semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);

    shmdt(dane);
    return 0;
}
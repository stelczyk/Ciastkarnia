#include "ciastkarnia.h"
#include <signal.h>

static DaneWspolne *dane = NULL;
static int sem_id = -1;
static int msg_kasa_id = -1; 
static volatile sig_atomic_t otrzymano_sigterm = 0;

void obsluga_sigterm(int sig) {
    otrzymano_sigterm = 1;
}

int main(){ 
    signal(SIGTERM, SIG_IGN);
    
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
        semafor_zablokuj(sem_id, SEM_OUTPUT);
        print_log(KOLOR_KLIENT"[KLIENT %d] Sklep pelny (%d/%d), czekam przed sklepem...\n"RESET, klientID, w_sklepie, MAX_KLIENTOW);
        semafor_odblokuj(sem_id, SEM_OUTPUT);
}

    //zablokuj semafor wejścia
    semafor_zablokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);

    //sprawdz czy sklep nadal otwarty i czy nie ma ewakuacji
    semafor_zablokuj(sem_id, SEM_MUTEX_DANE);

    if(!dane->sklep_otwarty || dane->zamykanie || dane->ewakuacja){
        semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
        semafor_odblokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);
        semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);
        semafor_zablokuj(sem_id, SEM_OUTPUT);
        if(dane->ewakuacja) {
            print_log(KOLOR_KLIENT"[KLIENT %d] EWAKUACJA! Nie wchodze do sklepu.\n"RESET, klientID);
        } else {
            print_log(KOLOR_KLIENT"[KLIENT %d] Sklep zamkniety, nie wchodze.\n"RESET, klientID);
        }
        semafor_odblokuj(sem_id, SEM_OUTPUT);

        shmdt(dane);
        exit(0);
    }

    //wejdz do sklepu
    dane->klienci_w_sklepie++;
    int ilu_w_sklepie = dane->klienci_w_sklepie;
    
    signal(SIGTERM, obsluga_sigterm);

    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log( KOLOR_KLIENT "[KLIENT %d] Wchodze do sklepu (klientow: %d/%d)\n"RESET, klientID, ilu_w_sklepie, MAX_KLIENTOW);
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    //otworz drugą kase jesli potrzeba
    if(ilu_w_sklepie >= PROG_DRUGIEJ_KASY && !dane->kasa_otwarta[1]){
        dane->kasa_otwarta[1] = 1;
        semafor_odblokuj(sem_id, SEM_DZIALANIE_KASY2);
        semafor_zablokuj(sem_id, SEM_OUTPUT);
        print_log("[SYSTEM] Otwieram kase 2,   Klientow: %d >= %d\n", ilu_w_sklepie, PROG_DRUGIEJ_KASY);
        semafor_odblokuj(sem_id, SEM_OUTPUT);

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

    //wyswietl liste zakupow
    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log(KOLOR_KLIENT "[KLIENT %d] Chce kupic: "RESET, klientID);
    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        if(listaZakupow[i] > 0){
            print_log(KOLOR_KLIENT"  - %s: %d szt."RESET, NAZWA_PRODUKTOW[i], listaZakupow[i]);
        }
    }

    print_log("\n");
    semafor_odblokuj(sem_id, SEM_OUTPUT);

  
    int koszyk[LICZBA_RODZAJOW];
    int suma = 0;

    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        koszyk[i] = 0;
    }

    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log(KOLOR_KLIENT "[KLIENT %d] Robie zakupy\n"RESET, klientID);
    semafor_odblokuj(sem_id, SEM_OUTPUT);
    
    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        //sprawdz ewakuacje
        if(dane->ewakuacja) {
            //wszystko w jednej sekcji krytycznej
            semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
            semafor_zablokuj(sem_id, SEM_OUTPUT);
            
            print_log(KOLOR_KLIENT "[KLIENT %d] EWAKUACJA! Odkładam towar do kosza: "RESET, klientID);
            int cos_mialem = 0;
            for(int j = 0; j < LICZBA_RODZAJOW; j++) {
                if(koszyk[j] > 0) {
                    print_log(KOLOR_KLIENT "  - %s: %d szt.\n"RESET, NAZWA_PRODUKTOW[j], koszyk[j]);
                    cos_mialem = 1;
                    dane->w_koszu[j] += koszyk[j];
                    dane->sprzedano[j] -= koszyk[j]; 
                }
            }
            if(!cos_mialem) {
                print_log(KOLOR_KLIENT "  (koszyk byl pusty)\n"RESET);
            }
            
            dane->klienci_w_sklepie--;
            int zostalo = dane->klienci_w_sklepie;
            print_log(KOLOR_KLIENT "[KLIENT %d] EWAKUACJA - wychodze! (zostalo klientow: %d)\n"RESET, klientID, zostalo);

            
            semafor_odblokuj(sem_id, SEM_OUTPUT);
            semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
            
            semafor_odblokuj(sem_id, SEM_ILOSC_KLIENTOW);
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
            int znaleziony_numer = -1;
            semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
            if(dane->feeder_head[i] != -1){
                int idx_towaru = dane->feeder_head[i];
                
                znaleziony_numer = dane->node_pool[idx_towaru].numer_sztuki;
                
                dane->feeder_head[i] = dane->node_pool[idx_towaru].next;
                if(dane->feeder_head[i] == -1){
                    dane->feeder_tail[i] = -1;
                }

                dane->node_pool[idx_towaru].next = dane->free_head;
                dane->free_head = idx_towaru;

                dane->sprzedano[i]++;
                dane->na_podajniku[i]--;
            }
            semafor_odblokuj(sem_id, SEM_MUTEX_DANE);

            if(znaleziony_numer == -1){
                 break;
            }



            wzialem++;
            koszyk[i]++;
            suma += CENY[i];
            
            //aktualizuj statystyki
            semafor_zablokuj(sem_id, SEM_OUTPUT);
            print_log(KOLOR_KLIENT "[KLIENT %d] Wzialem %s #%d\n"RESET, klientID, NAZWA_PRODUKTOW[i], znaleziony_numer);
            semafor_odblokuj(sem_id, SEM_OUTPUT);
        }

        //komunikat o braku towaru
        if(wzialem == 0){
            semafor_zablokuj(sem_id, SEM_OUTPUT);
            print_log(KOLOR_KLIENT "[KLIENT %d] Brak %s na podajniku!\n"RESET, klientID, NAZWA_PRODUKTOW[i]);
            semafor_odblokuj(sem_id, SEM_OUTPUT);
                   
        } else if(wzialem < chce){
            semafor_zablokuj(sem_id, SEM_OUTPUT);
            print_log(KOLOR_KLIENT "[KLIENT %d] Wzialem tylko %d/%d szt. %s\n"RESET, klientID, wzialem, chce, NAZWA_PRODUKTOW[i]);
            semafor_odblokuj(sem_id, SEM_OUTPUT);       
        }
        
    }

    //sprawdz ewakuacje po zakonczeniu zakupow
    if(dane->ewakuacja) {
        //wszystko w jednej sekcji krytycznej
        semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
        semafor_zablokuj(sem_id, SEM_OUTPUT);
        
        print_log(KOLOR_KLIENT "[KLIENT %d] EWAKUACJA! Odkładam towar do kosza:\n"RESET, klientID);
        int cos_mialem = 0;
        for(int j = 0; j < LICZBA_RODZAJOW; j++) {
            if(koszyk[j] > 0) {
                print_log(KOLOR_KLIENT "  - %s: %d szt.\n"RESET, NAZWA_PRODUKTOW[j], koszyk[j]);
                cos_mialem = 1;
                dane->w_koszu[j] += koszyk[j];
                dane->sprzedano[j] -= koszyk[j]; 
            }
        }
        if(!cos_mialem) {
            print_log(KOLOR_KLIENT "  (koszyk byl pusty)\n"RESET);
        }
        
        dane->klienci_w_sklepie--;
        int zostalo = dane->klienci_w_sklepie;
        print_log(KOLOR_KLIENT "[KLIENT %d] EWAKUACJA - wychodze! (zostalo klientow: %d)\n"RESET, klientID, zostalo);
        
        semafor_odblokuj(sem_id, SEM_OUTPUT);
        semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
        
        semafor_odblokuj(sem_id, SEM_ILOSC_KLIENTOW);
        semafor_odblokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);
        semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);
        shmdt(dane);
        exit(0);
    }

    //podsumowanie zakupow
    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log(KOLOR_KLIENT "[KLIENT %d] Skonczylem zakupy, w koszyku mam:"RESET, klientID);
    int ile_mam = 0;
    for(int i = 0; i < LICZBA_RODZAJOW; i++){
        if(koszyk[i] > 0){
            print_log(KOLOR_KLIENT "  - %s: %d szt."RESET, NAZWA_PRODUKTOW[i], koszyk[i]);
            ile_mam += koszyk[i];
        }
    }
    
    if(ile_mam == 0){
        print_log(KOLOR_KLIENT "  (nic)"RESET);
    }   
    print_log(KOLOR_KLIENT "  SUMA: %d zl\n"RESET, suma);
    print_log("\n");
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    //obsluga przy kasie
    int zostal_obsluzony = 0;
    if(suma > 0){
        
        semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
        semafor_zablokuj(sem_id, SEM_KOLEJKA_KASA1);
        semafor_zablokuj(sem_id, SEM_KOLEJKA_KASA2);

        //wybierz kase - kasa 1 zawsze dziala, kasa 2 zalezy od kasa_otwarta[1]
        int wybrana_kasa;
        if(dane->kasa_otwarta[1]){
            //obie kasy otwarte - wybierz krotsza kolejke
            if(dane->kasa_kolejka[1] < dane->kasa_kolejka[0]){
                wybrana_kasa = 2;
            }else{
                wybrana_kasa = 1;
            }
        } else {
            wybrana_kasa = 1; //tylko kasa 1
        }

        //dołacz do kolejki
        dane->kasa_kolejka[wybrana_kasa - 1]++;
        int moja_pozycja = dane->kasa_kolejka[wybrana_kasa - 1];

        semafor_odblokuj(sem_id, SEM_KOLEJKA_KASA2);
        semafor_odblokuj(sem_id, SEM_KOLEJKA_KASA1);
        semafor_odblokuj(sem_id, SEM_MUTEX_DANE);

        semafor_zablokuj(sem_id, SEM_OUTPUT);
        print_log(KOLOR_KLIENT "[KLIENT %d] Ide do kasy %d, pozycja w kolecje %d\n"RESET, klientID, wybrana_kasa, moja_pozycja);
        semafor_odblokuj(sem_id, SEM_OUTPUT);
        
        
      //sprawdz ewakuacje przed pojsciem do kasy 
if(dane->ewakuacja) {
    //zmniejsz kolejke i wypisz, wszystko w jednej sekcji krytycznej 
    semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
    semafor_zablokuj(sem_id, (wybrana_kasa == 1) ? SEM_KOLEJKA_KASA1 : SEM_KOLEJKA_KASA2);
    semafor_zablokuj(sem_id, SEM_OUTPUT);
    
    print_log(KOLOR_KLIENT"[KLIENT %d] EWAKUACJA przy kasie! Zostawiam koszyk:"RESET, klientID);
    int cos_mialem = 0;
    for (int j = 0; j < LICZBA_RODZAJOW; j++) {
        if (koszyk[j] > 0) {
            print_log(KOLOR_KLIENT"  - %s: %d szt."RESET, NAZWA_PRODUKTOW[j], koszyk[j]);
            cos_mialem = 1;
            dane->w_koszu[j] += koszyk[j];
            dane->sprzedano[j] -= koszyk[j]; 
        }
    }
    print_log("\n");
    if (!cos_mialem) {
        print_log(KOLOR_KLIENT"  (koszyk byl pusty)\n"RESET);
    }
    
    if (dane->kasa_kolejka[wybrana_kasa - 1] > 0) {
        dane->kasa_kolejka[wybrana_kasa - 1]--;
    }
    dane->klienci_w_sklepie--;
    int zostalo = dane->klienci_w_sklepie;
    
    print_log(KOLOR_KLIENT "[KLIENT %d] EWAKUACJA - wychodze! (zostalo klientow: %d)\n"RESET, klientID, zostalo);
    
    semafor_odblokuj(sem_id, SEM_OUTPUT);
    semafor_odblokuj(sem_id, (wybrana_kasa == 1) ? SEM_KOLEJKA_KASA1 : SEM_KOLEJKA_KASA2);
    semafor_odblokuj(sem_id, SEM_MUTEX_DANE);

    semafor_odblokuj(sem_id, SEM_ILOSC_KLIENTOW);
    semafor_odblokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);
    semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);
    shmdt(dane);
    exit(0);
}


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

        //czekaj na potwierdzenie od kasjera (blokujace msgrcv)
        MsgPotwierdzenie potwierdzenie;
        while(1) {
            //sprawdz ewakuacje przed blokowaniem
            if(dane->ewakuacja || otrzymano_sigterm) {
                semafor_zablokuj(sem_id, SEM_OUTPUT);
                print_log(KOLOR_KLIENT"[KLIENT %d] EWAKUACJA! Nie czekam na paragon, uciekam!\n"RESET, klientID);
                semafor_odblokuj(sem_id, SEM_OUTPUT);
                break;
            }
            
            //probuj odebrac potwierdzenie (blokujace)
            ssize_t wynik = msgrcv(msg_kasa_id, &potwierdzenie, sizeof(potwierdzenie) - sizeof(long), klientID, 0);
            
            if(wynik != -1) {
                if(potwierdzenie.obsluzony == 1) {
                    zostal_obsluzony = 1; //klient dostal paragon
                } else {
                    //nie ustawiamy flagi zostal_obsluzony
                }
                break; //otrzymano potwierdzenie
            }
            
            if(errno == EINTR) {
                //przerwane sygnalem - sprawdz czy ewakuacja
                if(dane->ewakuacja || otrzymano_sigterm) {
                    semafor_zablokuj(sem_id, SEM_OUTPUT);
                    print_log(KOLOR_KLIENT"[KLIENT %d] Sygnal ewakuacji przy kasie!\n"RESET, klientID);
                    semafor_odblokuj(sem_id, SEM_OUTPUT);
                    break;
                }
                continue; //inny sygnal, probuj ponownie
            }
            
            //inny blad - wyjdz
            break;
        }
    }

    //wyjscie ze sklepu
    semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
    int byla_ewakuacja = (dane->ewakuacja || otrzymano_sigterm) && !zostal_obsluzony;
    
    //jesli ewakuacja przy kasie i nie obsluzony - odloz towar
    if(byla_ewakuacja && suma > 0) {
        semafor_zablokuj(sem_id, SEM_OUTPUT);
        print_log(KOLOR_KLIENT "[KLIENT %d] EWAKUACJA przy kasie! Odkładam towar do kosza:"RESET, klientID);
        int cos_mialem = 0;
        for(int j = 0; j < LICZBA_RODZAJOW; j++) {
            if(koszyk[j] > 0) {
                print_log(KOLOR_KLIENT "  - %s: %d szt."RESET, NAZWA_PRODUKTOW[j], koszyk[j]);
                cos_mialem = 1;
                dane->w_koszu[j] += koszyk[j];
                dane->sprzedano[j] -= koszyk[j];
            }
        }
        print_log("\n");
        if(!cos_mialem) {
            print_log(KOLOR_KLIENT "  (koszyk byl pusty)\n"RESET);
        }
        semafor_odblokuj(sem_id, SEM_OUTPUT);
    }
    
    dane->klienci_w_sklepie--;
    int zostalo = dane->klienci_w_sklepie;

    semafor_zablokuj(sem_id, SEM_OUTPUT);
    if(byla_ewakuacja) {
        print_log(KOLOR_KLIENT "[KLIENT %d] EWAKUACJA - wychodze! (zostalo klientow: %d)\n"RESET, klientID, zostalo);
    } else {
        print_log(KOLOR_KLIENT "[KLIENT %d] Wychodze (zostalo klientow: %d)\n"RESET, klientID, zostalo);
    }
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    //zamknij kase 2 jesli malo klientow (tylko jesli nie ewakuacja)
    if(!byla_ewakuacja && zostalo < PROG_DRUGIEJ_KASY && dane->kasa_otwarta[1]){
        dane->kasa_otwarta[1] = 0;
        semafor_zablokuj(sem_id, SEM_OUTPUT);
        print_log("[SYSTEM] Zamykam kase 2,   Klientow: %d < %d\n", zostalo, PROG_DRUGIEJ_KASY);
        semafor_odblokuj(sem_id, SEM_OUTPUT);
    }

    semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
    semafor_odblokuj(sem_id, SEM_ILOSC_KLIENTOW); //sygnalizuj kierownikowi ze klient wyszedl
    semafor_odblokuj_bez_undo(sem_id, SEM_WEJSCIE_SKLEP);
    semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);

    shmdt(dane);
    return 0;
}

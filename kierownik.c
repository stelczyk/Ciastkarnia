#include "ciastkarnia.h"
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>

static int shm_id = -1;
static DaneWspolne *dane = NULL;
static int sem_id = -1;
static int msg_id = -1;
static int msg_kasa_id = -1;


void obsluga_inwentaryzacji(int sig){
    printf(KOLOR_KIEROWNIK"\n[KIEROWNIK] Otrzymalem sygnal INWENATRYZACJI\n"RESET);
    fflush(stdout);

    if(dane != NULL){
        dane->inwentaryzacja = 1;
    }
}

void obsluga_ewakuacji(int sig){
    printf(KOLOR_KIEROWNIK"\n[KIEROWNIK] Otrzymalem sygnal EWAKUACJI\n"RESET);
    fflush(stdout);

    if(dane != NULL){
        dane->ewakuacja = 1;
        dane->sklep_otwarty = 0;
    }
}

void generuj_raport(){
    int fd = open("raport.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd == -1){
        perror("[KIEROWNIK] Blad tworzenia pliku raportu");
        return;
    }
    char bufor[2048];
    int len = 0;

    len += sprintf(bufor + len, "RAPORT INWENTARYZACJI\n\n");
    len += sprintf(bufor + len, "WYPRODUKOWANO:\n");
    for(int i = 0; i < LICZBA_RODZAJOW; i++) {
        if(dane->wyprodukowano[i] > 0) {
            len += sprintf(bufor + len, "%s: %d szt.\n", NAZWA_PRODUKTOW[i], dane->wyprodukowano[i]);
        }
    }
    
    
    len += sprintf(bufor + len, "\nNA PODAJNIKACH:\n");
    for(int i = 0; i < LICZBA_RODZAJOW; i++) {
        if(dane->na_podajniku[i] > 0) {
            len += sprintf(bufor + len, "%s: %d szt.\n", NAZWA_PRODUKTOW[i], dane->na_podajniku[i]);
        }
    }
    
    
    len += sprintf(bufor + len, "\nSPRZEDANO:\n");
    for(int i = 0; i < LICZBA_RODZAJOW; i++) {
        if(dane->sprzedano[i] > 0) {
            len += sprintf(bufor + len, "%s: %d szt.\n", NAZWA_PRODUKTOW[i], dane->sprzedano[i]);
        }
    }
    
    
    len += sprintf(bufor + len, "\nKASY:\n");
    len += sprintf(bufor + len, "Kasa 1: %d klientow\n", dane->kasa_obsluzonych[0]);
    len += sprintf(bufor + len, "Kasa 2: %d klientow\n", dane->kasa_obsluzonych[1]);
    
    
    int suma_kosz = 0;
    for(int i = 0; i < LICZBA_RODZAJOW; i++) {
        suma_kosz += dane->w_koszu[i];
    }
    if(suma_kosz > 0) {
        len += sprintf(bufor + len, "\nKOSZ (ewakuacja):\n");
        for(int i = 0; i < LICZBA_RODZAJOW; i++) {
            if(dane->w_koszu[i] > 0) {
                len += sprintf(bufor + len, "%s: %d szt.\n", NAZWA_PRODUKTOW[i], dane->w_koszu[i]);
            }
        }
    }
    
    write(fd, bufor, len);
    close(fd);
    
    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Raport zapisano do: raport.txt\n"RESET);
    fflush(stdout);
}

void sprzatanie(int sig) {
    printf(KOLOR_KIEROWNIK"\n\n[KIEROWNIK] Otrzymano sygnal‚ zakonczenia\n"RESET);
    fflush(stdout);

    if(dane != NULL){
        dane->sklep_otwarty = 0;
        dane->piekarnia_otwarta = 0;
    }
    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Zamykam sklep i zwalniam pracownikow...\n"RESET);
    fflush(stdout);
    
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);
    
    while(wait(NULL) > 0); 

    if(dane != NULL){
        if(shmdt(dane) == -1){
            perror("blad shmdt");
        }
        printf("[KIEROWNIK] Odlaczono pamiec dzielona\n");
        fflush(stdout);
    }

    if(shm_id >= 0){
        if(shmctl(shm_id, IPC_RMID, NULL) == -1){
            perror("blad shmctl");
        }
        printf("[KIEROWNIK] Usunieto pamiec dzielona\n");
        fflush(stdout);
    }

    if(msg_id >= 0){
        if(msgctl(msg_id, IPC_RMID, NULL) == -1){
            perror("[KIEROWNIK] Blad msgctl IPC_RMID");
        }
        printf("[KIEROWNIK] Usunieto kolejke komunikatow\n");
        fflush(stdout);
    }

    if(msg_kasa_id >= 0){
        if(msgctl(msg_kasa_id, IPC_RMID, NULL) == -1){
            perror("[KIEROWNIK] Blad msgctl IPC_RMID dla kas");
        }
        printf("[KIEROWNIK] Usunieto kolejke kas\n");
        fflush(stdout);
    }

    if(sem_id >= 0){
        if(semctl(sem_id, 0, IPC_RMID) == -1){
            perror("Blad semctl IPC_RMID");
        }
        printf("[KIEROWNIK] Usunieto grupe semaforowa\n");
        fflush(stdout);
    }
    
    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Sklep zamkniety. Do widzenia.\n"RESET);
    fflush(stdout);
    exit(0);
}

int main(){
    signal(SIGINT, sprzatanie);
    signal(SIGTERM, sprzatanie); 
    signal(SIGCHLD, SIG_IGN);
    signal(SYGNAL_INWENTARYZACJA, obsluga_inwentaryzacji);
    signal(SYGNAL_EWAKUACJA, obsluga_ewakuacji);


    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Otwieram ciastkarnie\n"RESET);
    fflush(stdout);

    key_t klucz = ftok(SCIEZKA_KLUCZA, PROJ_ID_SHM);
    if(klucz == -1){
        perror("blad ftok");
        exit(1);
    }

    //printf("[KIEROWNIK] Klucz IPC: %d\n", klucz);

    shm_id = shmget(klucz, sizeof(DaneWspolne), IPC_CREAT | 0600);
    if(shm_id == -1){
        perror("blad shmget");
        exit(1);
    }

    //printf("[Kierownik] Pamiec dzielona ID: %d\n", shm_id);

    dane = (DaneWspolne *)shmat(shm_id, NULL, 0);
    if(dane == (void *) -1){
        perror("blad shmat");
        exit(1);
    }

    memset(dane, 0, sizeof(DaneWspolne));
    dane->sklep_otwarty = 1;
    dane->piekarnia_otwarta = 1;

    dane->kasa_otwarta[0] = 1;
    dane->kasa_otwarta[1] = 0;
    dane->kasa_kolejka[0] = 0;
    dane->kasa_kolejka[1] = 0;
    dane->kasa_obsluzonych[0] = 0;
    dane->kasa_obsluzonych[1] = 0;

    //printf("[Kierownik] Pamiec dzielona gotowa\n");

    sem_id = utworz_grupe_semaforowa();
    if(sem_id == -1){
        perror("[KIEROWNIK] Blad tworzenia grupy semaforowej");
        exit(1);
    }

    if(ustaw_wartosci_semaforow(sem_id) == -1){
        perror("[KIEROWNIK] Blad ustawienia wartosci semaforow");
        exit(1);
    }

    //printf("[KIEROWNIK] Grupa semaforowa utworzona, ID: %d\n", sem_id);
    //fflush(stdout);

    key_t klucz_msg = ftok(SCIEZKA_KLUCZA, PROJ_ID_MSG);
    if (klucz_msg == -1){
        perror("[KIEROWNIK] Blad ftok dla kolejki");
        exit(1);
    }
    //printf("[KIEROWNIK] Klucz kolejki: %d\n", klucz_msg);

    msg_id = msgget(klucz_msg, IPC_CREAT | 0600);
    if(msg_id == -1){
        perror("[KIEROWNIK] Blad msgget");
        exit(1);
    }
    //printf("[KIEROWNIK] Kolejka komunikatow ID: %d\n", msg_id);

    

    key_t klucz_msg_kasa = ftok(SCIEZKA_KLUCZA,PROJ_ID_MSG_KASA);
        if(klucz_msg_kasa == -1){
            perror("[KIEROWNIK] Blad ftok dla kolejki kas");
            exit(1);
    }

    msg_kasa_id = msgget(klucz_msg_kasa, IPC_CREAT | 0600);
    if(msg_kasa_id == -1){
        perror("[KIEROWNIK] Blad msgget dla kas");
    }

    pid_t piekarz = fork();
    if (piekarz == 0){
        execl("./piekarz", "piekarz", NULL);
        perror("Nie udalo sie uruchomic piekarza");
        exit(1);
    }
    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Zatrudnilem piekarza %d\n"RESET, piekarz);
    fflush(stdout);

    pid_t kasjer1 = fork();
    if(kasjer1 == 0){
        execl("./kasjer", "kasjer", "1", NULL);
        perror("Nie udalo sie uruchomic kasjera 1");
        exit(1);
    }
    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Zatrudnilem kasjera 1 PID: %d\n"RESET, kasjer1);
    fflush(stdout);
    dane->kasjer1_pid = kasjer1;


    pid_t kasjer2 = fork();
    if(kasjer2 == 0){
        execl("./kasjer", "kasjer", "2", NULL);
        perror("Nie udalo sie uruchomic kasjera 2");
        exit(1);
    }
    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Zatrudnilem kasjera 2 PID: %d\n"RESET, kasjer2);
    fflush(stdout);
     dane->kasjer2_pid = kasjer2;

    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Piekarnia otwarta! Sklep otworzy sie za %d sekund\n"RESET, CZAS_PRZED_OTWARCIEM_SKLEPU);
    fflush(stdout);

    sleep(CZAS_PRZED_OTWARCIEM_SKLEPU);
    
    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Sklep otwarty!\n"RESET);
    fflush(stdout);

    time_t czas_startu = time(NULL);
    time_t czas_konca = czas_startu + CZAS_PRACY_SKLEPU;

    while(1){
        time_t teraz = time(NULL);
        int pozostalo = (int)(czas_konca - teraz);


        if(teraz >= czas_konca || dane->ewakuacja){ //sprawdza czy czas pracy minal lub ewakuacja
           if (dane->ewakuacja) {
                printf(KOLOR_KIEROWNIK"\n[KIEROWNIK] EWAKUACJA! Zamykam sklep.\n"RESET); //rozne komunikaty
            } else {
                printf(KOLOR_KIEROWNIK"\n[KIEROWNIK] Czas pracy minął! Zamykam sklep.\n"RESET);
            }
            fflush(stdout);

            dane->zamykanie = 1; //flagi zamykania
            dane->piekarnia_otwarta = 0;
            printf(KOLOR_KIEROWNIK"[KIEROWNIK] Czekam az klienci skoncza zakupy...\n"RESET);
            fflush(stdout);

            int ostatnia_ilosc = -1; //czeka na wyjscie wszystkich klientow
            while(dane->klienci_w_sklepie > 0){
                if(dane->klienci_w_sklepie != ostatnia_ilosc){
                    ostatnia_ilosc = dane->klienci_w_sklepie;
                    semafor_zablokuj(sem_id, SEM_OUTPUT);
                    printf(KOLOR_KIEROWNIK"[KIEROWNIK] Pozostalo klientow %d\n"RESET, dane->klienci_w_sklepie);
                    fflush(stdout);     
                    semafor_odblokuj(sem_id, SEM_OUTPUT);
                }
                
                //usleep(100000);
            }
            dane->sklep_otwarty = 0;
            printf(KOLOR_KIEROWNIK"[KIEROWNIK] Wszyscy klienci wyszli, zamykam sklep\n"RESET);
            fflush(stdout);
            
            //generuje raport przy inwentaryzacji
            if(dane->inwentaryzacja){
                printf(KOLOR_KIEROWNIK"[KIEROWNIK] Generuje raport\n"RESET);
                fflush(stdout);
                generuj_raport();
            }
            break;
        }
        //xdusleep(10000);

        //sleep(losowanie(1,2));

        if(dane->sklep_otwarty && !dane->zamykanie){ //generuj nowego klienta jesli otwarty sklep
            semafor_zablokuj_bez_undo(sem_id, SEM_MAX_PROCESOW); //ogranicza liczbe procesow

            if(!dane->sklep_otwarty || dane->zamykanie){
                semafor_odblokuj_bez_undo(sem_id, SEM_MAX_PROCESOW);
            }else{
         
                pid_t klient = fork();
                if(klient == 0){
                    execl("./klient", "klient", NULL);
                    perror("Nie udalo sie uruchomic klienta");
                    exit(1);
                }
            }
      
        }
    }
    sprzatanie(0);
    return 0;
}
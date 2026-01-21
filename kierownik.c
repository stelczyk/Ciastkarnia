#include "ciastkarnia.h"
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>

static int shm_id = -1;
static DaneWspolne *dane = NULL;
static int sem_id = -1;
static int msg_kasa_id = -1;

void handler_alarm(int sig) { }


void obsluga_inwentaryzacji(int sig){
    

    if(dane != NULL){
        dane->inwentaryzacja = 1;
    }
}

void obsluga_ewakuacji(int sig){
    if(dane != NULL){
        dane->ewakuacja = 1;
        dane->zamykanie = 1;
        dane->sklep_otwarty = 0;
    }
    //wyslij sygnal do wszystkich procesow w grupie, aby przerwac blokady na semaforach
    kill(0, SIGTERM);

 
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
    
    if(write(fd, bufor, len) == -1){
        perror("[KIEROWNIK] Blad write do pliku raportu");
    }
    close(fd);
    
    if(sem_id >= 0) {
        semafor_zablokuj(sem_id, SEM_OUTPUT);
        print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Raport zapisano do: raport.txt\n"RESET);
        semafor_odblokuj(sem_id, SEM_OUTPUT);
    } else {
        print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Raport zapisano do: raport.txt\n"RESET);
    }
}

static pid_t parent_pid = -1;

void sprzatanie(int sig) {
    if(parent_pid != -1 && getpid() != parent_pid){
        _exit(0);
    }
    
    if(dane != NULL){
        dane->sklep_otwarty = 0;
        dane->piekarnia_otwarta = 0;
        dane->zamykanie = 1;
    }

    if(sig != 0) {
        print_log(KOLOR_KIEROWNIK"\n\n[KIEROWNIK] Otrzymano sygnal zakonczenia\n"RESET);
    } else {
        print_log(KOLOR_KIEROWNIK"\n\n[KIEROWNIK] Procedura zamkniecia sklepu\n"RESET);
    }

    print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Zamykam sklep i zwalniam pracownikow...\n"RESET);
    
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);
    
    while(wait(NULL) > 0); 

    //generuj raport jesli byla inwentaryzacja
    if(dane != NULL && dane->inwentaryzacja){
        if(sem_id >= 0) {
            semafor_zablokuj(sem_id, SEM_OUTPUT);
            print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Generuje raport\n"RESET);
            semafor_odblokuj(sem_id, SEM_OUTPUT);
        } else {
            print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Generuje raport\n"RESET);
        }
        generuj_raport();
    }

    if(dane != NULL){
        if(shmdt(dane) == -1){
            perror("blad shmdt");
        }
        print_log("[KIEROWNIK] Odlaczono pamiec dzielona\n");
    }

    if(shm_id >= 0){
        if(shmctl(shm_id, IPC_RMID, NULL) == -1){
            perror("blad shmctl");
        }
        print_log("[KIEROWNIK] Usunieto pamiec dzielona\n");
    }


    if(msg_kasa_id >= 0){
        if(msgctl(msg_kasa_id, IPC_RMID, NULL) == -1){
            perror("[KIEROWNIK] Blad msgctl IPC_RMID dla kas");
        }
        print_log("[KIEROWNIK] Usunieto kolejke kas\n");
    }

    if(sem_id >= 0){
        if(semctl(sem_id, 0, IPC_RMID) == -1){
            perror("Blad semctl IPC_RMID");
        }
        print_log("[KIEROWNIK] Usunieto grupe semaforowa\n");
    }
    
    print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Sklep zamkniety. Do widzenia.\n"RESET);
    exit(0);
}

int main(){
    parent_pid = getpid();
    signal(SIGINT, sprzatanie);
    signal(SIGTERM, sprzatanie); 
    signal(SIGCHLD, SIG_IGN);
    signal(SYGNAL_INWENTARYZACJA, obsluga_inwentaryzacji);
    signal(SYGNAL_EWAKUACJA, obsluga_ewakuacji);
    signal(SIGALRM, handler_alarm);


    FILE *f_clean = fopen("symulacja.txt", "w");
    if(f_clean) fclose(f_clean);
    
    f_clean = fopen("kasjer_log.txt", "w");
    if(f_clean) fclose(f_clean);

    print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Otwieram ciastkarnie\n"RESET);

    key_t klucz = ftok(SCIEZKA_KLUCZA, PROJ_ID_SHM);
    if(klucz == -1){
        perror("blad ftok");
        exit(1);
    }

    shm_id = shmget(klucz, sizeof(DaneWspolne), IPC_CREAT | 0600);
    if(shm_id == -1){
        perror("blad shmget");
        exit(1);
    }

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


    sem_id = utworz_grupe_semaforowa();
    if(sem_id == -1){
        perror("[KIEROWNIK] Blad tworzenia grupy semaforowej");
        exit(1);
    }

    if(ustaw_wartosci_semaforow(sem_id) == -1){
        perror("[KIEROWNIK] Blad ustawienia wartosci semaforow");
        exit(1);
    }


    for(int i = 0; i<MAX_NODES - 1;  i++){
        dane->node_pool[i].next = i + 1;
    }
    dane->node_pool[MAX_NODES - 1].next = -1;
    dane->free_head = 0;

    for(int i = 0; i<LICZBA_RODZAJOW; i++){
        dane->feeder_head[i] = -1;
        dane->feeder_tail[i] = -1;
    }   

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
    if(piekarz == -1){
        perror("[KIEROWNIK] Blad fork piekarza");
        exit(1);
    }
    if (piekarz == 0){
        execl("./piekarz", "piekarz", NULL);
        perror("Nie udalo sie uruchomic piekarza");
        exit(1);
    }
    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Zatrudnilem piekarza %d\n"RESET, piekarz);
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    pid_t kasjer1 = fork();
    if(kasjer1 == -1){
        perror("[KIEROWNIK] Blad fork kasjera 1");
        exit(1);
    }
    if(kasjer1 == 0){
        execl("./kasjer", "kasjer", "1", NULL);
        perror("Nie udalo sie uruchomic kasjera 1");
        exit(1);
    }
    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Zatrudnilem kasjera 1 PID: %d\n"RESET, kasjer1);
    semafor_odblokuj(sem_id, SEM_OUTPUT);
   


    pid_t kasjer2 = fork();
    if(kasjer2 == -1){
        perror("[KIEROWNIK] Blad fork kasjera 2");
        exit(1);
    }
    if(kasjer2 == 0){
        execl("./kasjer", "kasjer", "2", NULL);
        perror("Nie udalo sie uruchomic kasjera 2");
        exit(1);
    }
    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Zatrudnilem kasjera 2 PID: %d\n"RESET, kasjer2);
    semafor_odblokuj(sem_id, SEM_OUTPUT);
 

    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Piekarnia otwarta! Sklep otworzy sie za %d sekund\n"RESET, CZAS_PRZED_OTWARCIEM_SKLEPU);
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    alarm(CZAS_PRZED_OTWARCIEM_SKLEPU);
    pause();
    
    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Sklep otwarty!\n"RESET);
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    time_t czas_startu = time(NULL);
    time_t czas_konca = czas_startu + CZAS_PRACY_SKLEPU;
    int liczba_stworzonych_klientow = 0;

    while(1){
        time_t teraz = time(NULL);



        if(teraz >= czas_konca || dane->ewakuacja){ //sprawdza czy czas pracy minal lub ewakuacja
            dane->zamykanie = 1;
            dane->piekarnia_otwarta = 0;
            
            semafor_zablokuj(sem_id, SEM_OUTPUT);
           if (dane->ewakuacja) {
                print_log(KOLOR_KIEROWNIK"\n[KIEROWNIK] EWAKUACJA! Zamykam sklep.\n"RESET); //rozne komunikaty
            } else {
                print_log(KOLOR_KIEROWNIK"\n[KIEROWNIK] Czas pracy minal! Zamykam sklep.\n"RESET);
            }

            print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Czekam az klienci skoncza zakupy...\n"RESET);
            semafor_odblokuj(sem_id, SEM_OUTPUT);

            //czeka na wyjscie wszystkich klientow
            while(1){
                semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
                int klientow = dane->klienci_w_sklepie;
                semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
                
                if(klientow <= 0) break;
                
                //czekaj na zmiane liczby klientow przez semafor
                semafor_zablokuj(sem_id, SEM_ILOSC_KLIENTOW);
            }
            dane->sklep_otwarty = 0;
            semafor_zablokuj(sem_id, SEM_OUTPUT);
            print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Wszyscy klienci wyszli, zamykam sklep\n"RESET);
            print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Laczna liczba stworzonych klientow: %d\n"RESET, liczba_stworzonych_klientow);
            
            //generuje raport przy inwentaryzacji
            if(dane->inwentaryzacja){
                print_log(KOLOR_KIEROWNIK"[KIEROWNIK] Generuje raport\n"RESET);
            }
            semafor_odblokuj(sem_id, SEM_OUTPUT);
            if(dane->inwentaryzacja){
                generuj_raport();
            }
            break;
        }
        if(dane->sklep_otwarty && !dane->zamykanie && !dane->ewakuacja){ //generuj nowego klienta jesli otwarty sklep
            int mozna_tworzyc = dane->sklep_otwarty && !dane->zamykanie && !dane->ewakuacja;
            
            if(mozna_tworzyc){
                //probuj pobrac semafor nieblokujaco
                struct sembuf op;
                op.sem_num = SEM_MAX_PROCESOW;
                op.sem_op = -1;
                op.sem_flg = IPC_NOWAIT;
                if(semop(sem_id, &op, 1) == 0){
                    pid_t klient = fork();
                    if(klient == -1){
                        perror("[KIEROWNIK] Blad fork klienta");
                        continue;
                    }
                    if(klient == 0){
                        execl("./klient", "klient", NULL);
                        perror("Nie udalo sie uruchomic klienta");
                        exit(1);
                    }
                    liczba_stworzonych_klientow++;
                }
            }
        }

        
    }
    sprzatanie(0);
    return 0;
}
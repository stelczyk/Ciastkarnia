#include "ciastkarnia.h"
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

static int shm_id = -1;
static DaneWspolne *dane = NULL;
static int sem_id = -1;


void sprzatanie(int sig) {
    printf("\n\n[KIEROWNIK] Otrzymano sygnał zakończenia\n");

    if(dane != NULL){
        dane->sklep_otwarty = 0;
        dane->piekarnia_otwarta = 0;
    }
    printf("[KIEROWNIK] Zamykam sklep i zwalniam pracowników...\n");
    
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);
    
    while(wait(NULL) > 0); 

    if(dane != NULL){
        if(shmdt(dane) == -1){
            perror("blad shmdt");
        }
        printf("[KIEROWNIK] Odlaczono pamiec dzielona\n");
    }

    if(shm_id >= 0){
        if(shmctl(shm_id, IPC_RMID, NULL) == -1){
            perror("blad shmctl");
        }
        printf("[KIEROWNIK] Usunieto pamiec dzielona\n");
    }

    if(sem_id >= 0){
        if(semctl(sem_id,0, IPC_RMID) == -1){
            perror("Blad semctl IPC_RMID");
        }
        printf("[KIEROWNIK] Usunieto semafor\n");
    }
    
    printf("[KIEROWNIK] Sklep zamknięty. Do widzenia.\n");
    exit(0);
}

int main(){
    signal(SIGINT, sprzatanie);
    signal(SIGCHLD, SIG_IGN);

    printf("[KIEROWNIK] Otwieram ciastkarnie\n");

    key_t klucz = ftok(SCIEZKA_KLUCZA, PROJ_ID_SHM);
    if(klucz == -1){
        perror("blad ftok");
        exit(1);
    }

    printf("[KIEROWNIK] Klucz IPC: %d\n", klucz);

    shm_id = shmget(klucz, sizeof(DaneWspolne), IPC_CREAT | 0600);
    if(shm_id == -1){
        perror("blad shmget");
        exit(1);
    }

    printf("[Kierownik] Pamiec dzielona ID: %d\n", shm_id);

    dane = (DaneWspolne *)shmat(shm_id, NULL, 0);
    if(dane == (void *) -1){
        perror("blad shmat");
        exit(1);
    }

    memset(dane, 0, sizeof(DaneWspolne));
    dane->sklep_otwarty = 1;
    dane->piekarnia_otwarta = 1;
    printf("[Kierownik] Pamiec dzielona gotowa\n");

    key_t klucz_sem = ftok(SCIEZKA_KLUCZA, PROJ_ID_SEM);
    if (klucz_sem == -1){
        perror("[KIEROWNIK] Blad ftok semafor");
        exit(1);
    }

    printf("[KIEROWNIK] Klucz semafora: %d\n", klucz_sem);

    sem_id = semget(klucz_sem, 1, IPC_CREAT | 0600);
    if(sem_id == -1){
        perror("[KIEROWNIK] Blad semget");
        exit(1);
    }
    printf("[KIEROWNIK] Semafor ID: %d\n", sem_id);

    if(semctl(sem_id, 0, SETVAL,1) == -1){
        perror("[KIEROWNIK] Blad semctl");
        exit(1);
    }
    printf("[KIEROWNIK] Semafor ustawiony na 1 (odblokowany)\n");


    pid_t piekarz = fork();
    if (piekarz == 0){
        execl("./piekarz", "piekarz", NULL);
        perror("Nie udalo sie uruchomic piekarza");
        exit(1);
    }
    printf("[KIEROWNIK] Zatrudnilem piekarza %d", piekarz);

    while(1){
        sleep(3);
        pid_t klient = fork();
        if(klient == 0){
            execl("./klient", "klient", NULL);
            perror("Nie udalo sie uruchomic klienta");
            exit(1);
        }
    }
    return 0;
}
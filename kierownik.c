#include "ciastkarnia.h"
#include <sys/wait.h>
#include <signal.h>


void sprzatanie(int sig) {
    printf("\n\n[KIEROWNIK] Otrzymano sygnał zakończenia\n");
    printf("[KIEROWNIK] Zamykam sklep i zwalniam pracowników...\n");
    
    kill(0, SIGTERM);
    
    while(wait(NULL) > 0); 
    
    printf("[KIEROWNIK] Sklep zamknięty. Do widzenia.\n");
    exit(0);
}

int main(){
    signal(SIGINT, sprzatanie);

    signal(SIGCHLD, SIG_IGN);

    printf("[KIEROWNIK ] Otwieram ciastkarnie\n");

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
#include "ciastkarnia.h"

int main(){
    srand(time(NULL));

    pid_t piekarzID = getpid();

    printf("[PIEKARZ %d] Rozpoczynam prace\n", piekarzID);

    while(1){
        int indeks = losowanie(0,LICZBA_RODZAJOW - 1);
        int czasPieczenia = losowanie(1,3);
        int ilosc = losowanie(2,10);

        printf("[PIEKARZ %d] Rozpoczynam pieczenie %s\n", piekarzID, NAZWA_PRODUKTOW[indeks]);
        sleep(czasPieczenia);
        printf("[PIEKARZ %d] Upieklem %d sztuk %s\n", piekarzID, ilosc, NAZWA_PRODUKTOW[indeks]);

        usleep(500000);

        printf("[PIEKARZ %d] Wykladam %s na podajnik\n", piekarzID,NAZWA_PRODUKTOW[indeks]);

        sleep(1);
}
    return 0;
}
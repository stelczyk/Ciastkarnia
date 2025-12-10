#include "ciastkarnia.h"

int main(){
    srand(time(NULL));
    pid_t klientID = getpid();

    printf("[KLIENT %d] Wchodze co ciastkarni\n", klientID);
    sleep(1);

    int ileProduktow = losowanie(2,5);
    int koszykID[5];
    int koszykIlosc[5];
    int zapelnienie = 0;

    while( zapelnienie < ileProduktow){
        int chceKupic = losowanie(0,LICZBA_RODZAJOW - 1);
        int wKoszyku = 0;
        for(int i = 0;i < zapelnienie; i++){
            if(koszykID[i] == chceKupic){
                wKoszyku = 1;
                break;
            }
        }

        if(wKoszyku == 0){
            koszykID[zapelnienie] = chceKupic;
            koszykIlosc[zapelnienie] = losowanie(2,4);
            zapelnienie++;
        }
    }

    printf("[KLIENT %d] Kupuje takie produkty: \n", klientID);
    int suma = 0;
    for(int i = 0; i< zapelnienie; i++){
        int id = koszykID[i];
        int ilosc = koszykIlosc[i];
        int wartosc = ilosc * CENY[id];
        printf("%s: %d sztuk x %d  = %d PLN\n", NAZWA_PRODUKTOW[id], ilosc, CENY[id], wartosc);

        suma += wartosc;
    }
    printf("[Klient %d] Do zaplaty: %d\n", klientID, suma);

    return 0;
}
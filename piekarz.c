#include "ciastkarnia.h"



static int sem_id = -1;
static DaneWspolne *dane = NULL;
static int licznik_produktow = 0;

int main(){
    srand(time(NULL));

    pid_t piekarzID = getpid();

    key_t klucz = ftok(SCIEZKA_KLUCZA, PROJ_ID_SHM);
    if(klucz == -1){
        perror("[PIEKARZ] Blad ftok");
        exit(1);
    }

    int shm_id = shmget(klucz, sizeof(DaneWspolne), 0600);
    if(shm_id == -1){
        perror("[PIEKARZ] Blad shmget");
        exit(1);
    }

    dane = (DaneWspolne *)shmat(shm_id, NULL, 0);
    if(dane == (void*)-1){
        perror("[PIEKARZ] Blad shmat");
        exit(1);
    }


   sem_id = pobierz_grupe_semaforowa();
   if(sem_id == -1){
        perror("[PIEKARZ] Blad pobierania grupy semaforowej");
        exit(1);
   }

    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log( KOLOR_PIEKARZ"[PIEKARZ %d] Rozpoczynam prace\n" RESET, piekarzID);
    semafor_odblokuj(sem_id, SEM_OUTPUT);

    while(dane->piekarnia_otwarta){ //sprawdza czy nie ma ewakuacji
        if(dane->ewakuacja) {
            semafor_zablokuj(sem_id, SEM_OUTPUT);
            print_log(KOLOR_PIEKARZ"[PIEKARZ %d] EWAKUACJA! Koncze prace!\n"RESET, piekarzID);
            semafor_odblokuj(sem_id, SEM_OUTPUT);
            break;
        }
        
        //wybierz losowy produkt ktory ma miejsce na podajniku
        int indeks = losowanie(0, LICZBA_RODZAJOW - 1);
        int proby = 0;
        
        semafor_zablokuj(sem_id, SEM_MUTEX_DANE);
        //szukaj produktu z wolnym miejscem (zacznij od losowego)
        while(proby < LICZBA_RODZAJOW) {
            int zapas = dane->na_podajniku[indeks];
            int limit = POJEMNOSC_PODAJNIKA[indeks];
            if(zapas < limit) {
                break; //znaleziono produkt z miejscem
            }
            indeks = (indeks + 1) % LICZBA_RODZAJOW;
            proby++;
        }
        semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
        
        int ilosc = losowanie(2, 5);
        
        //piecze kazda sztuke
        for (int i = 0; i < ilosc; i++){
            //sprawdz warunki wyjscia
            if(!dane->piekarnia_otwarta || dane->ewakuacja) break;

            //sprawdz czy ten konkretny podajnik ma miejsce
            semafor_zablokuj(sem_id, SEM_MUTEX_DANE);

            int aktualnie = dane->na_podajniku[indeks];
            int limit = POJEMNOSC_PODAJNIKA[indeks];

            if(aktualnie >= limit){
                //ten podajnik pelny, probuj inny produkt
                semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
                break;
            }

            //przygotuj produkt do wyslania
            licznik_produktow++;
            if(dane->free_head == -1){
                licznik_produktow--;
                semafor_odblokuj(sem_id, SEM_MUTEX_DANE);
                break;  
            }
            
            int nowy_idx = dane->free_head;
            dane->free_head = dane->node_pool[nowy_idx].next;
            
            dane->node_pool[nowy_idx].type = indeks;
            dane->node_pool[nowy_idx].numer_sztuki = licznik_produktow;
            dane->node_pool[nowy_idx].next = -1;
            if(dane->feeder_head[indeks] == -1){
                dane->feeder_head[indeks] = nowy_idx;
                dane->feeder_tail[indeks] = nowy_idx;
            }else{
                int stary_ogon = dane->feeder_tail[indeks];
                dane->node_pool[stary_ogon].next = nowy_idx;
                dane->feeder_tail[indeks] = nowy_idx;
            }
            
            
            //aktualizuj statystyki - atomowo z wysylaniem
            dane->wyprodukowano[indeks]++;
            dane->na_podajniku[indeks]++;
            int ile_teraz = dane->na_podajniku[indeks];

            semafor_odblokuj(sem_id, SEM_MUTEX_DANE);

            semafor_zablokuj(sem_id, SEM_OUTPUT);
            print_log(KOLOR_PIEKARZ"[PIEKARZ %d] Wyslalem %s #%d na podajnik (%d/%d)\n"RESET,
                   piekarzID, NAZWA_PRODUKTOW[indeks], licznik_produktow, ile_teraz, POJEMNOSC_PODAJNIKA[indeks]);
            semafor_odblokuj(sem_id, SEM_OUTPUT);
        }
    }

    semafor_zablokuj(sem_id, SEM_OUTPUT);
    print_log(KOLOR_PIEKARZ"[PIEKARZ %d] Piekarnia zamknieta, koncze prace\n"RESET, piekarzID);
    semafor_odblokuj(sem_id, SEM_OUTPUT);
    shmdt(dane);
    return 0;
}
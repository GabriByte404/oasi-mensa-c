#ifndef COMMON_H
    #define COMMON_H

    /**
    * @brief Dizionario principale del progetto Mensa.
    */

    #include <sys/types.h>
    #include <time.h>
    #include <unistd.h> 
    #include <limits.h> 

    // --- 1. Costanti e Enum ---

    typedef enum {
        PRIMI = 0,
        SECONDI = 1,
        COFFEE = 2,
        CASSA = 3
    } StationType;

    #define NOF_STATIONS 4
    #define MAX_EXTRA_USERS 1000

    // Per Primi e Secondi useremo solo i primi 2 slot, per il caffè ne useremo 4.
    #define MAX_DISHES_PER_TYPE 4 

    // --- 2. Indici Semafori ---
    #define SEM_STAT_MUTEX      0 
    #define SEM_RESP_SYNC       1 

    // Mutex Stazioni
    #define SEM_PRIMI_MUTEX     2
    #define SEM_SECONDI_MUTEX   3
    #define SEM_COFFEE_MUTEX    4
    #define SEM_CASSA_MUTEX     5

    // Posti liberi operatori (per il Worker che cerca lavoro)
    #define SEM_WK_PRIMI_SEATS   6 
    #define SEM_WK_SECONDI_SEATS 7
    #define SEM_WK_COFFEE_SEATS  8
    #define SEM_WK_CASSA_SEATS   9 

    // Posti Tavoli Utenti
    #define SEM_TABLE_SEATS     10 

    // Attesa gruppi
    #define SEM_GROUP_BARRIER   11

    #define TOTAL_SEMAPHORES    12 

    #define IPC_KEY_FILE_PATH "./ipc_key_anchor"

    // ID per ftok
    #define MSQ_ID_ASSIGNMENT_KEY 101 
    #define SHM_ID_ASSIGNMENT_KEY 65 
    #define SEM_ID_ASSIGNMENT_KEY 99 

    // --- 3. Strutture Dati ---
    typedef struct {
        // Generali
        int SIM_DURATION;
        int OVERLOAD_THRESHOLD;
        long N_NANO_SECS;
        int NOF_WORKERS;
        int NOF_USERS;
        
        // Posti
        int NOF_WK_SEATS_PRIMI;
        int NOF_WK_SEATS_SECONDI;
        int NOF_WK_SEATS_COFFEE;
        int NOF_WK_SEATS_CASSA;
        int NOF_TABLE_SEATS;

        // Tempi di Servizio (AVG_SRVC)
        int AVG_SRVC_PRIMI;
        int AVG_SRVC_SECONDI;
        int AVG_SRVC_COFFEE;
        int AVG_SRVC_CASSA;

        // Prezzi
        int PRICE_PRIMI;
        int PRICE_SECONDI;
        int PRICE_COFFEE;

        // Refill
        int REFILL_THRESHOLD; 
        int REFILL_AMOUNT;    
        int AVG_REFILL_TIME;  
        int MAX_PORZIONI_PRIMI;   
        int MAX_PORZIONI_SECONDI; 
        int AVG_REFILL_PRIMI;   // start
        int AVG_REFILL_SECONDI;   // start

        // Pause
        int NOF_PAUSES; // Numero massimo di pause consentite

        // Gruppi
        int MAX_USERS_PER_GROUP;

    } ConfigData;

    typedef struct {
        int station_id;
        int available_seats; 
        
        // Dish portions: Per il caffè sarà impostato a un valore altissimo (infinito)
        int dish_portions[MAX_DISHES_PER_TYPE];

        // Statistiche
        double total_wait_time_ns;
        long served_dishes_total;
        int active_workers;
    } Station;

    typedef struct {
        ConfigData config;
        Station stations[NOF_STATIONS];
        int available_table_seats; 

        // Numero reale di piatti contati dal menu
        int menu_count_primi;
        int menu_count_secondi;
        int menu_count_coffee;
        
        // Statistiche Globali
        int served_users_total; 
        int unserved_users_total;   
        double total_revenue; 
        int total_pauses; 

        // --- REGISTRAZIONE EXTRA USERS ---
        pid_t extra_users_pids[MAX_EXTRA_USERS];
        int extra_users_count;
        
        // --- LOGICA GRUPPI ---
        int group_members_count[2000];    
        int group_ready_count[2000];

        // Controllo
        int current_day;
    } GlobalState;

    // --- 4. Messaggi ---

    // Pagamento (Utente -> Cassa)
    typedef struct {
        long mtype;          // Deve essere 1
        pid_t user_pid;      // Chi sta pagando
        int num_primi;       // Quanti primi ha preso
        int num_secondi;     // Quanti secondi ha preso
        int num_coffee;      // Quanti caffè ha preso
        double total_to_pay; // O calcolato dal cassiere, o inviato dall'utente
    } PaymentMessage;

    // --- COLORI ANSI PER TERMINALE ---
    #define RESET   "\033[0m"
    #define RED     "\033[31m"      // Errori / Criticità
    #define GREEN   "\033[32m"      // Successo / Cassa / Mangiare
    #define YELLOW  "\033[33m"      // Attesa / Code
    #define BLUE    "\033[34m"      // Operatori Primi/Secondi
    #define MAGENTA "\033[35m"      // Responsabile / Eventi speciali
    #define CYAN    "\033[36m"      // Utenti generici
    #define BOLD    "\033[1m"       // Grassetto

#endif
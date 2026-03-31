#include <stdio.h>      // Per printf
#include <stdlib.h>     // Per exit, malloc
#include <unistd.h>     // Per usleep, sleep
#include <sys/types.h>  // Per pid_t
#include <string.h>     // strncmp, strlen

// --- LIBRERIE PROGETTO ---
#include "simulation_utils.h"
#include "../shared/ipc_utils.h"      // Per sem_wait, sem_signal, set_sem_value
#include "process_utils.h"            // Per spawn_process
#include "../shared/common.h"         // Per macro (PRIMI, SECONDI...) e GlobalState

// --- VARIABILI ESTERNE ---
extern int sem_id;
extern int shm_id;
extern int msq_id;
extern GlobalState *state;
extern pid_t *child_pids;

// --- FUNZIONI STATICHE (Helper interni) ---


static int count_menu_items(const char *filename, const char *prefix) {
    FILE *f = fopen(filename, "r");
    if (!f) return 0; // File non trovato, 0 piatti

    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        // Cerca la riga che inizia con il prefisso (es. "PRIMI=")
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            count = 1; // Se la riga esiste, c'è almeno un piatto (quello iniziale)
            char *ptr = line;
            while (*ptr) {
                if (*ptr == ',') count++; // Ogni virgola aggiunge un piatto
                ptr++;
            }
            break; // Trovato, stop lettura
        }
    }
    fclose(f);
    
    // Safety check: non superare la dimensione dell'array in memoria condivisa
    if (count > MAX_DISHES_PER_TYPE) count = MAX_DISHES_PER_TYPE;
    
    return count;
}

// --- IMPLEMENTAZIONI ---

// 1. REFILL MATTUTINO
void perform_daily_refill() {
    // Primi
    sem_wait(sem_id, SEM_PRIMI_MUTEX);
    for(int i=0; i<MAX_DISHES_PER_TYPE; i++) {
        if(state->stations[PRIMI].dish_portions[i] != -1){
            state->stations[PRIMI].dish_portions[i] = state->config.AVG_REFILL_PRIMI;
        }
    }
    sem_signal(sem_id, SEM_PRIMI_MUTEX);

    // Secondi
    sem_wait(sem_id, SEM_SECONDI_MUTEX);
    for(int i=0; i<MAX_DISHES_PER_TYPE; i++) {
        if(state->stations[SECONDI].dish_portions[i] != -1){
            state->stations[SECONDI].dish_portions[i] = state->config.AVG_REFILL_SECONDI;
        }
    }
    sem_signal(sem_id, SEM_SECONDI_MUTEX);
    
    printf("Responsabile: Pentole riempite per il nuovo giorno.\n");
}

// 2. INIZIALIZZAZIONE STATO
void init_simulation_state() {
    // A. Init Statistiche
    state->served_users_total = 0;
    state->unserved_users_total = 0;
    state->total_revenue = 0.0;
    state->total_pauses = 0;
    state->current_day = 0;

    // 1. CONTEGGIO RIGOROSO
    int n_primi   = count_menu_items("./config/menu.txt", "PRIMI:"); 
    int n_secondi = count_menu_items("./config/menu.txt", "SECONDI:");
    int n_coffee  = count_menu_items("./config/menu.txt", "COFFEE:");

    // 2. CONTROLLO BLOCCANTE CON PULIZIA
    if (n_primi == 0) {
        fprintf(stderr, "\n" RED "[ERRORE CRITICO] Nessun 'PRIMO' trovato nel menu!" RESET "\n");
        fprintf(stderr, "Impossibile aprire la mensa. Verifica ./config/menu.txt\n");
        
        // PULIZIA PRIMA DI USCIRE
        cleanup_ipc_resources(shm_id, sem_id, msq_id);
        exit(EXIT_FAILURE); 
    }
    
    if (n_secondi == 0) {
        fprintf(stderr, "\n" RED "[ERRORE CRITICO] Nessun 'SECONDO' trovato nel menu!" RESET "\n");
        
        // PULIZIA PRIMA DI USCIRE
        cleanup_ipc_resources(shm_id, sem_id, msq_id);
        exit(EXIT_FAILURE);
    }

    // Salva nello state
    state->menu_count_primi = n_primi;
    state->menu_count_secondi = n_secondi;
    state->menu_count_coffee = (n_coffee > 0) ? n_coffee : 1; 

    printf("[INIT] Menu Valido: %d Primi, %d Secondi. La mensa può aprire.\n", n_primi, n_secondi);

    // B. Init Stazioni e Piatti
    for (int i = 0; i < NOF_STATIONS; i++) {
        state->stations[i].station_id = i;
        state->stations[i].served_dishes_total = 0;
        state->stations[i].total_wait_time_ns = 0;
        state->stations[i].active_workers = 0;

        int real_dishes_count = 0;
        int initial_portions = 0;
        int is_unlimited = 0; 

        if (i == PRIMI) { 
            initial_portions = state->config.AVG_REFILL_PRIMI; 
            real_dishes_count = state->menu_count_primi; 
        } 
        else if (i == SECONDI) { 
            initial_portions = state->config.AVG_REFILL_SECONDI; 
            real_dishes_count = state->menu_count_secondi;
        } 
        else if (i == COFFEE) { 
            is_unlimited = 1; 
            real_dishes_count = state->menu_count_coffee;
        } 
        else if (i == CASSA) {
            real_dishes_count = 0;
        }

        for(int d = 0; d < MAX_DISHES_PER_TYPE; d++){
            if (d < real_dishes_count) {
                state->stations[i].dish_portions[d] = is_unlimited ? 9999 : initial_portions;
            } else {
                state->stations[i].dish_portions[d] = -1; 
            }
        }
    }

    // C. Init Semafori
    set_sem_value(sem_id, SEM_STAT_MUTEX, 1);
    set_sem_value(sem_id, SEM_RESP_SYNC, 0);    
    set_sem_value(sem_id, SEM_PRIMI_MUTEX, 1);
    set_sem_value(sem_id, SEM_SECONDI_MUTEX, 1);
    set_sem_value(sem_id, SEM_COFFEE_MUTEX, 1);
    set_sem_value(sem_id, SEM_CASSA_MUTEX, 1);
    set_sem_value(sem_id, SEM_TABLE_SEATS, state->config.NOF_TABLE_SEATS);
    set_sem_value(sem_id, SEM_WK_PRIMI_SEATS, state->config.NOF_WK_SEATS_PRIMI);
    set_sem_value(sem_id, SEM_WK_SECONDI_SEATS, state->config.NOF_WK_SEATS_SECONDI);
    set_sem_value(sem_id, SEM_WK_COFFEE_SEATS, state->config.NOF_WK_SEATS_COFFEE);
    set_sem_value(sem_id, SEM_WK_CASSA_SEATS, state->config.NOF_WK_SEATS_CASSA);
    set_sem_value(sem_id, SEM_GROUP_BARRIER, 0);

    printf("Responsabile: Memoria e Semafori inizializzati.\n");

    state->extra_users_count = 0;
    for(int i=0; i<2000; i++) {
        state->group_members_count[i] = 0;
        state->group_ready_count[i] = 0;
    }
    
}

// 3. SPAWN PROCESSI COMPLESSO
int spawn_simulation_processes() {
    int p_idx = 0;
    
    // 1. Logica Priorità Operatori
    printf("Responsabile: Calcolo assegnazione operatori (%d)...\n", state->config.NOF_WORKERS);
    char *types_str[] = {"PRIMI", "SECONDI", "COFFEE", "CASSA"};
    
    int seats_limit[4];
    seats_limit[PRIMI]   = state->config.NOF_WK_SEATS_PRIMI;
    seats_limit[SECONDI] = state->config.NOF_WK_SEATS_SECONDI;
    seats_limit[COFFEE]  = state->config.NOF_WK_SEATS_COFFEE;
    seats_limit[CASSA]   = state->config.NOF_WK_SEATS_CASSA;
    int assigned_count[4] = {0, 0, 0, 0};

    // Ordinamento priorità
    int priority[4] = {PRIMI, SECONDI, COFFEE, CASSA};
    int times[4] = {state->config.AVG_SRVC_PRIMI, state->config.AVG_SRVC_SECONDI, state->config.AVG_SRVC_COFFEE, state->config.AVG_SRVC_CASSA};

    // Bubble sort priorità
    for(int i=0; i<4; i++) {
        for(int j=i+1; j<4; j++) {
            if(times[priority[j]] > times[priority[i]]) {
                int temp = priority[i]; priority[i] = priority[j]; priority[j] = temp;
            }
        }
    }

    // Spawn Operatori
    for (int i = 0; i < state->config.NOF_WORKERS; i++) {
        int target_station = -1;
        if (i < 4) { target_station = i; } // Fase 1: Minimo garantito
        else {
            for (int p = 0; p < 4; p++) { // Fase 2: Priorità con posti liberi
                int staz_idx = priority[p];
                if (assigned_count[staz_idx] < seats_limit[staz_idx]) {
                    target_station = staz_idx; break;
                }
            }
            if (target_station == -1) target_station = i % 4; // Fallback (Round Robin)
        }
        child_pids[p_idx++] = spawn_process("./bin/operatore", types_str[target_station]);
        assigned_count[target_station]++;
    }

    // Spawn Utenti
    printf("Responsabile: Generazione %d UTENTI in GRUPPI...\n", state->config.NOF_USERS);
    int users_spawned = 0;
    int group_id_counter = 1;
    char gid_str[10];

    while(users_spawned < state->config.NOF_USERS) {
        // Dimensione gruppo random (1 a MAX_USERS_PER_GROUP)
        int g_size = (rand() % state->config.MAX_USERS_PER_GROUP) + 1;
        if (users_spawned + g_size > state->config.NOF_USERS){
            g_size = state->config.NOF_USERS - users_spawned;
        }
        
        state->group_members_count[group_id_counter] = g_size;
        sprintf(gid_str, "%d", group_id_counter);

        for(int k=0; k<g_size; k++) {
            child_pids[p_idx++] = spawn_process("./bin/utente", gid_str);
            users_spawned++;
        }
        group_id_counter++;
    }
    
    return p_idx;
}

void reset_groups_for_new_day() {
    sem_wait(sem_id, SEM_STAT_MUTEX);
    for(int i = 0; i < 2000; i++) {
        state->group_ready_count[i] = 0; // Azzera i pronti, ma NON i membri totali
    }
    sem_signal(sem_id, SEM_STAT_MUTEX);
}
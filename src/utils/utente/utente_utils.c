#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/msg.h>

#include "utente_utils.h"
#include "../shared/ipc_utils.h"

// --- Helper Locale (Privato) ---
long get_random_delay_nanos(int avg_seconds, int percent_variation, long nanos_per_min) {
    if (avg_seconds <= 0) return 0;
    double variation = (avg_seconds * percent_variation) / 100.0;
    double min_sec = avg_seconds - variation;
    double max_sec = avg_seconds + variation;
    double random_sec = min_sec + ((double)rand() / RAND_MAX) * (max_sec - min_sec);
    double nanos_per_sec = (double)nanos_per_min / 60.0;
    return (long)(random_sec * nanos_per_sec);
}

// --- IMPLEMENTAZIONI ---

void menu_selection(MenuChoice *scelta, GlobalState *state){
    FILE *f = fopen(MENU_FILE, "r");
    
    // Fallback se file non esiste
    if(f == NULL){
        scelta->voglio_primo = 1; scelta->id_primo = rand() % state->menu_count_primi;
        scelta->voglio_secondo = 1; scelta->id_secondo = rand() % state->menu_count_secondi;
        scelta->voglio_coffee = rand() % 2; 
        if(scelta->voglio_coffee) scelta->id_coffee = rand() % state->menu_count_coffee;
        return;
    }
    
    // Reset
    scelta->voglio_primo = 0;
    scelta->voglio_secondo = 0;
    scelta->voglio_coffee = 0;
    char line[256]; 

    while(fgets(line, sizeof(line), f)){
        if (strstr(line, "PRIMI:")) {   
            scelta->voglio_primo = 1;    
            scelta->id_primo = rand() % state->menu_count_primi;  
        }
        if (strstr(line, "SECONDI:")) {
            scelta->voglio_secondo = 1;
            scelta->id_secondo = rand() % state->menu_count_secondi; 
        }
        if (strstr(line, "COFFEE:")) {  
            if(rand() % 2 == 1){ // 50% probabilità
                scelta->voglio_coffee = 1;
                scelta->id_coffee = rand() % state->menu_count_coffee;
            } 
        }
    }
    fclose(f);

    // Sicurezza: Almeno un piatto principale
    if (!scelta->voglio_primo && !scelta->voglio_secondo) {
        scelta->voglio_primo = 1; scelta->id_primo = rand() % state->menu_count_primi;
        scelta->voglio_secondo = 1; scelta->id_secondo = rand() % state->menu_count_secondi;
    }
}  

int take_food(int sem_id, GlobalState *state, int station_type, int dish_id, const char *tag) {
    int mutex_id;
    int avg_srvc, variation_pct;
    char *name_staz;

    // Configurazione Parametri
    switch(station_type){
        case PRIMI:   mutex_id = SEM_PRIMI_MUTEX;   avg_srvc = state->config.AVG_SRVC_PRIMI;   variation_pct = 50; name_staz="PRIMI"; break;
        case SECONDI: mutex_id = SEM_SECONDI_MUTEX; avg_srvc = state->config.AVG_SRVC_SECONDI; variation_pct = 50; name_staz="SECONDI"; break;
        case COFFEE:  mutex_id = SEM_COFFEE_MUTEX;  avg_srvc = state->config.AVG_SRVC_COFFEE;  variation_pct = 80; name_staz="COFFEE"; break;
        default: return 0;
    }

    printf("%s In coda per %s...\n", tag, name_staz);

    // A. Misurazione Attesa
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start); 

    // B. Entrata Zona Critica
    sem_wait(sem_id, mutex_id);
    
    clock_gettime(CLOCK_MONOTONIC, &t_end);   
    double wait_ns = (t_end.tv_sec - t_start.tv_sec) * 1e9 + (t_end.tv_nsec - t_start.tv_nsec);
    state->stations[station_type].total_wait_time_ns += wait_ns; // Accumulo ns

    printf("%s Servito a %s (Attesa: %.3f ms). Attendo preparazione...\n", tag, name_staz, wait_ns/1e6);
    
    // D. Simulazione Servizio
    long service_nanos = get_random_delay_nanos(avg_srvc, variation_pct, state->config.N_NANO_SECS);
    struct timespec ts = {service_nanos / 1000000000L, service_nanos % 1000000000L};
    nanosleep(&ts, NULL);

    int success = 0;
    int chosen_dish = dish_id;
    
    // Fallback Piatto (se finito)
    if (state->stations[station_type].dish_portions[chosen_dish] <= 0 && station_type != COFFEE) {
        for (int i = 0; i < MAX_DISHES_PER_TYPE; i++) {
            if (state->stations[station_type].dish_portions[i] > 0) {
                chosen_dish = i;
                printf("%s Piatto preferito finito, ripiego su ID %d.\n", tag, i);
                break;
            }
        }
    }

    // Prelievo effettivo
    if (state->stations[station_type].dish_portions[chosen_dish] > 0 || station_type == COFFEE) {
        if (station_type != COFFEE) {
            state->stations[station_type].dish_portions[chosen_dish]--;
        }
        success = 1;
        state->stations[station_type].served_dishes_total++; 
        printf("%s Preso piatto ID %d da " GREEN "%s" RESET ".\n", tag, chosen_dish, name_staz);
    } 
    else {
        printf("%s " RED "%s: CIBO FINITO!" RESET " (Esco a mani vuote)\n", tag, name_staz);
    }

    // F. Uscita Zona Critica
    sem_signal(sem_id, mutex_id);

    usleep(10000); 
    return success;
}

void process_payment(int msq_id, pid_t user_pid, int n_primi, int n_secondi, int n_coffee, const char *tag, ConfigData config, int has_ticket) {
    double total_amount = (n_primi * config.PRICE_PRIMI) + 
                          (n_secondi * config.PRICE_SECONDI) + 
                          (n_coffee * config.PRICE_COFFEE);
    
    printf("%s Vado in cassa (%.2f eur)...\n", tag, total_amount);
    
    PaymentMessage msg;
    msg.mtype = 1;  
    msg.user_pid = user_pid;
    msg.num_primi = n_primi;
    msg.num_secondi = n_secondi;
    msg.num_coffee = n_coffee;

    // APPLICAZIONE SCONTO TICKET
    if (has_ticket) {
        double discount = total_amount * 0.20; // Sconto del 20%
        total_amount -= discount;
        printf("%s " YELLOW "Ticket validato! Sconto applicato. Totale: %.2f" RESET "\n", tag, total_amount);
    } else {
        printf("%s Totale da pagare alla cassa: %.2f\n", tag, total_amount);
    }

    msg.total_to_pay = total_amount;
    
    // 1. INVIO (Request)
    if(msgsnd(msq_id, &msg, sizeof(PaymentMessage) - sizeof(long), 0) == -1){
        perror("Errore invio pagamento");
        return;
    }

    // 2. ATTESA (REPLY)
    // Mi blocco qui finché il Cassiere non mi risponde sul canale "Mio PID"
    PaymentMessage receipt;
    if (msgrcv(msq_id, &receipt, sizeof(PaymentMessage)-sizeof(long), user_pid, 0) == -1) {
        perror("Errore attesa scontrino");
    } else {
        printf("%s Pagamento confermato! Vado a mangiare.\n", tag);
    }
}

void consume_meal(int sem_id, GlobalState *state, const char *tag) {
    int sem_val = semctl(sem_id, SEM_TABLE_SEATS, GETVAL);
    if(sem_val == 0) {
        printf("%s " YELLOW "TAVOLI PIENI!" RESET " Aspetto che qualcuno si alzi...\n", tag);
    }

    sem_wait(sem_id, SEM_TABLE_SEATS);
    
    int eating_duration_sim_minutes = 20 + (rand() % 11); 
    long total_nanos = (long)eating_duration_sim_minutes * state->config.N_NANO_SECS;
    
    printf("%s Trovato tavolo! " GREEN "Mangio per %d min" RESET " (simulati)...\n", tag, eating_duration_sim_minutes);
    struct timespec req = {total_nanos / 1000000000L, total_nanos % 1000000000L};
    nanosleep(&req, NULL);

    sem_signal(sem_id, SEM_TABLE_SEATS);
}
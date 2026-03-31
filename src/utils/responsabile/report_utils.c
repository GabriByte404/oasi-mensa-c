#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "report_utils.h"
#include "../shared/ipc_utils.h"

// --- HELPER FUNZIONI ---

int get_current_leftovers(GlobalState *state, int station_type) {
    int total = 0;
    for(int i = 0; i < MAX_DISHES_PER_TYPE; i++) {
        if(state->stations[station_type].dish_portions[i] > 0) {
            total += state->stations[station_type].dish_portions[i];
        }
    }
    return total;
}

void print_report_row(const char* label, double val_today, double val_total, double val_avg, int is_currency) {
    if (is_currency) {
        printf("%-22s %7.2f € (Oggi) | %7.2f € (Totale) | %7.2f € (Media/Giorno)\t |\n", 
               label, val_today, val_total, val_avg);
    } else {
        printf("%-22s %7.0f   (Oggi) | %7.0f   (Totale) | %7.1f   (Media/Giorno)\t |\n", 
               label, val_today, val_total, val_avg);
    }
}

double calc_avg_wait(double total_wait_ns, long total_served, double ns_per_sim_min) {
    if (total_served <= 0) return 0.0;
    // Calcolo: Tempo Reale / Fattore di Conversione
    return (total_wait_ns / total_served) / ns_per_sim_min;
}

// --- SAVE CSV ---
void save_csv_report(DailyStatsSnapshot *s) {
    // Apro il file. Usa "a" (append) per non cancellare la storia precedente.
    FILE *f = fopen("stats.csv", "a");
    if (!f) return;

    // --- 1. Scrittura Header (Solo se il file è vuoto) ---
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0) {
        fprintf(f, "Giorno,Incasso_Oggi,Incasso_Tot,Serviti_Oggi,Serviti_Tot,Persi_Oggi,Persi_Tot,,Primi_Tot,Sec_Oggi,Sec_Tot,Caffe_Oggi,Caffe_Tot,Av_Primi_Oggi,Av_Sec_Oggi,Wait_Primi,Wait_Sec,Wait_Caffe,Pause_Oggi,Pause_Tot\n");
    }

    // --- 2. Scrittura Dati ---
    fprintf(f, "%d,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%d,%d\n",
        s->day_display,
        s->revenue_today, s->revenue_tot,
        s->served_today, s->served_tot,
        s->unserved_today, s->unserved_tot,
        s->dishes_today[PRIMI], s->dishes_tot[PRIMI],
        s->dishes_today[SECONDI], s->dishes_tot[SECONDI],
        s->dishes_today[COFFEE], s->dishes_tot[COFFEE],
        s->leftovers[0], s->leftovers[1],
        s->wait_avg[PRIMI], s->wait_avg[SECONDI], s->wait_avg[COFFEE],
        s->pauses_today, s->pauses_tot
    );

    fclose(f);
}

// --- MAIN REPORT FUNCTION ---
void print_daily_report(int day, GlobalState *state, int sem_id) {
    // 1. LOCK
    sem_wait(sem_id, SEM_STAT_MUTEX);
    
    // Memoria Storica
    static ReportHistory history = {0}; 
    
    // Inizializzazione Snapshot
    DailyStatsSnapshot stats = {0};
    stats.day_display = day + 1;

    // --- 2. POPOLAMENTO SNAPSHOT (Calcoli) ---
    
    // A. Economia e Utenti
    stats.revenue_tot = state->total_revenue;
    stats.revenue_today = stats.revenue_tot - history.revenue;

    stats.served_tot = state->served_users_total;
    stats.served_today = stats.served_tot - history.served;

    stats.unserved_tot = state->unserved_users_total;
    stats.unserved_today = stats.unserved_tot - history.unserved;

    stats.pauses_tot = state->total_pauses;
    stats.pauses_today = stats.pauses_tot - history.pauses;

    // B. Piatti e Attese per Stazione
    double sim_conv = (double)state->config.N_NANO_SECS;
    stats.dishes_count_all = 0; // Totale piatti serviti (somma categorie)
    stats.wait_tot_ns_all = 0;  // Totale tempo attesa (somma categorie)

    for(int i=0; i<NOF_STATIONS; i++) {
        if(i != CASSA) {
            // Totali (da SHM)
            stats.dishes_tot[i] = state->stations[i].served_dishes_total;
            // Oggi (Delta)
            stats.dishes_today[i] = stats.dishes_tot[i] - history.dishes[i];
            
            // Media Attesa (su Totale)
            stats.wait_avg[i] = calc_avg_wait(state->stations[i].total_wait_time_ns, stats.dishes_tot[i], sim_conv);
            
            // Accumulatori Globali
            stats.dishes_count_all += stats.dishes_tot[i];
            stats.wait_tot_ns_all += state->stations[i].total_wait_time_ns;
        }
    }

    // C. Avanzi (Snapshot Istantaneo)
    stats.leftovers[0] = get_current_leftovers(state, PRIMI);
    stats.leftovers[1] = get_current_leftovers(state, SECONDI);
    long left_today_sum = stats.leftovers[0] + stats.leftovers[1];

    // Aggiornamento accumulatore storico per la media avanzi
    history.acc_leftover_tot += left_today_sum;
    history.acc_leftover_primi += stats.leftovers[0];
    history.acc_leftover_secondi += stats.leftovers[1];


    // --- 3. STAMPA REPORT (Usa i dati calcolati in 'stats') ---
    printf("\n==========================================================================================\n");
    printf("                            REPORT MENSA - GIORNO %d\n", stats.day_display);
    printf("==========================================================================================\n");

    printf("--- [1] UTENTI & ECONOMIA ---\n");
    print_report_row("Incasso:", stats.revenue_today, stats.revenue_tot, stats.revenue_tot / stats.day_display, 1);
    print_report_row("Utenti Serviti:", (double)stats.served_today, (double)stats.served_tot, (double)stats.served_tot / stats.day_display, 0);
    print_report_row("Utenti Non Serviti:", (double)stats.unserved_today, (double)stats.unserved_tot, (double)stats.unserved_tot / stats.day_display, 0);

    printf("\n--- [2] PIATTI DISTRIBUITI ---\n");
    // Totale piatti oggi (calcolato al volo per display)
    int dishes_today_sum = stats.dishes_today[PRIMI] + stats.dishes_today[SECONDI] + stats.dishes_today[COFFEE];
    
    print_report_row("Totale:", (double)dishes_today_sum, (double)stats.dishes_count_all, (double)stats.dishes_count_all / stats.day_display, 0);
    
    const char* labels[] = {" > Primi:", " > Secondi:", " > Caffè:"};
    for(int i=0; i<3; i++) {
        print_report_row(labels[i], (double)stats.dishes_today[i], (double)stats.dishes_tot[i], (double)stats.dishes_tot[i] / stats.day_display, 0);
    }

    printf("\n--- [3] PIATTI AVANZATI (SPRECHI) ---\n");
    print_report_row("Totale Avanzi:", (double)left_today_sum, (double)history.acc_leftover_tot, (double)history.acc_leftover_tot/stats.day_display, 0);
    print_report_row(" > Primi:", (double)stats.leftovers[0], (double)history.acc_leftover_primi, (double)history.acc_leftover_primi/stats.day_display, 0);
    print_report_row(" > Secondi:", (double)stats.leftovers[1], (double)history.acc_leftover_secondi, (double)history.acc_leftover_secondi/stats.day_display, 0);
    printf("%-22s %s\n", " > Caffè:", "ILLIMITATO");

    printf("\n--- [4] TEMPI DI ATTESA (Minuti Simulati) ---\n");
    printf("Totale (Tutte Staz.):  %7.4f   (Totale Simulazione)\n", 
           calc_avg_wait(stats.wait_tot_ns_all, stats.dishes_count_all, sim_conv));

    for(int i=0; i<3; i++) {
        printf("%-22s %7.4f   (Totale Simulazione)\n", labels[i], stats.wait_avg[i]);
    }

    printf("\n--- [5] PERSONALE ---\n");
    int active_now = 0; 
    for(int i=0; i<NOF_STATIONS; i++) active_now += state->stations[i].active_workers;
    printf("Operatori Attivi Ora:  %d (su %d totali)\n", active_now, state->config.NOF_WORKERS);
    
    print_report_row("Pause:", (double)stats.pauses_today, (double)stats.pauses_tot, (double)stats.pauses_tot / stats.day_display, 0);

    printf("==========================================================================================\n\n");

    // --- 4. SALVATAGGIO CSV (Struct completa) ---
    save_csv_report(&stats);

    // --- 5. AGGIORNAMENTO STORICO (Per domani) ---
    history.revenue = stats.revenue_tot;
    history.served = stats.served_tot;
    history.unserved = stats.unserved_tot;
    history.pauses = stats.pauses_tot;
    
    for(int i=0; i<NOF_STATIONS; i++) {
        if(i != CASSA) {
            history.dishes[i] = stats.dishes_tot[i];
            history.wait_time[i] = state->stations[i].total_wait_time_ns;
        }
    }

    sem_signal(sem_id, SEM_STAT_MUTEX);
}
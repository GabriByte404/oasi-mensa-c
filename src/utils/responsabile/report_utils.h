/**
 * @file report_utils.h
 * @brief Funzioni di supporto per la reportistica del processo Responsabile.
 * * Questo modulo gestisce la raccolta, la visualizzazione (console) e 
 * la persistenza (CSV) delle statistiche giornaliere della mensa.
 * Implementa il pattern DTO (Data Transfer Object) per separare la logica
 * di estrazione dati da quella di formattazione.
 */

#ifndef RESPONSABILE_UTILS_H
#define RESPONSABILE_UTILS_H

    #include <sys/types.h>
    #include <stdio.h>       // Necessario per FILE*
    #include "../shared/common.h"

    /**
     * @struct DailyStatsSnapshot
     * @brief Struttura per i dati statistici di una singola giornata.
     * * Questa struttura serve a "fotografare" lo stato del sistema in un preciso istante
     * (alla fine del giorno), raccogliendo tutti i dati necessari in un unico oggetto.
     * Questo evita di dover ricalcolare o rileggere la memoria condivisa più volte
     * (una per la stampa a video, una per il CSV), garantendo coerenza tra i due output.
     */
    typedef struct {
        int day_display;            /**< Giorno (1, 2, 3...) */
        
        double revenue_tot;         /**< Incasso Totale */
        double revenue_today;       /**< Incasso Oggi */
        
        int served_tot;             /**< Serviti Totali */
        int served_today;           /**< Serviti Oggi */
        
        int unserved_tot;           /**< Persi Totali */
        int unserved_today;         /**< Persi Oggi */
        
        int dishes_tot[3];          /**< Piatti Totali [P, S, C] */
        int dishes_today[3];        /**< Piatti Oggi [P, S, C] */
        
        int leftovers[2];           /**< Avanzi Attuali [P, S] (È uno stato, non un cumulativo) */
        
        double wait_avg[3];         /**< Attesa Media (Minuti Simulati) */
        double wait_tot_ns_all;     /**< Somma attese (ns) per calcolo totale globale */
        long dishes_count_all;      /**< Somma piatti serviti per calcolo totale globale */
        
        int pauses_tot;             /**< Pause Totali */
        int pauses_today;           /**< Pause Oggi */
    } DailyStatsSnapshot;


    /**
     * @struct ReportHistory
     * @brief Struttura per memorizzare lo snapshot dei dati del giorno precedente.
     * * Essenziale per calcolare i valori "delta" (cioè quanto è successo SOLO oggi).
     * Poiché la Shared Memory contiene solo i totali accumulati, questa struct
     * funge da memoria storica per fare la differenza: (Valore_Oggi - Valore_Ieri).
     */
    typedef struct {
        double revenue;                 /**< Incasso totale accumulato fino a ieri. */
        int served;                     /**< Utenti serviti fino a ieri. */
        int unserved;                   /**< Utenti persi fino a ieri. */
        int pauses;                     /**< Pause totali fino a ieri. */
        
        int dishes[NOF_STATIONS];       /**< Totale piatti serviti per stazione fino a ieri. */
        double wait_time[NOF_STATIONS]; /**< Tempo attesa accumulato per stazione fino a ieri (ns). */
        
        long acc_leftover_tot;          /**< Accumulatore avanzi totali (per media storica). */
        long acc_leftover_primi;        /**< Accumulatore avanzi Primi. */
        long acc_leftover_secondi;      /**< Accumulatore avanzi Secondi. */
    } ReportHistory;


    /**
     * @brief Calcola e stampa il report statistico completo (Video + CSV).
     * * Funzione principale che orchestra il reporting:
     * 1. Acquisisce il lock sulle statistiche.
     * 2. Raccoglie i dati correnti in una `DailyStatsSnapshot`.
     * 3. Stampa il report formattato su stdout.
     * 4. Chiama `save_csv_report` passando lo snapshot.
     * 5. Aggiorna lo storico (`ReportHistory`) per il giorno successivo.
     * * @param day L'indice del giorno corrente (0-based).
     * @param state Puntatore alla memoria condivisa (GlobalState).
     * @param sem_id ID del set di semafori (per la mutua esclusione).
     */
    void print_daily_report(int day, GlobalState *state, int sem_id);


    /**
     * @brief Helper: Calcola gli avanzi correnti nelle pentole di una stazione.
     * * Scorre l'array `dish_portions` della stazione specificata e somma le porzioni rimaste.
     * * @param state Puntatore allo stato globale.
     * @param station_type Tipo di stazione da analizzare (PRIMI o SECONDI).
     * @return int Numero totale di porzioni attualmente disponibili (non servite).
     */
    int get_current_leftovers(GlobalState *state, int station_type);


    /**
     * @brief Helper: Formatta e stampa una riga della tabella report.
     * * Gestisce l'allineamento delle colonne "Oggi", "Totale", "Media".
     * * @param label Etichetta della riga (es. "Incasso:").
     * @param val_today Valore calcolato per oggi (Delta).
     * @param val_total Valore cumulativo totale.
     * @param val_avg Valore medio giornaliero.
     * @param is_currency Flag booleano: 1 per formato valuta (€), 0 per numeri standard.
     */
    void print_report_row(const char* label, double val_today, double val_total, double val_avg, int is_currency);


    /**
     * @brief Helper: Calcola il tempo medio di attesa in minuti simulati.
     * * Converte i nanosecondi totali in minuti simulati basandosi sul fattore
     * di conversione `ns_per_sim_min` definito nella configurazione.
     * * @param total_wait_ns Tempo totale attesa (nanosecondi).
     * @param total_served Numero totale utenti serviti.
     * @param ns_per_sim_min Fattore di scala temporale (ns reali = 1 minuto simulato).
     * @return double Tempo medio in minuti simulati.
     */
    double calc_avg_wait(double total_wait_ns, long total_served, double ns_per_sim_min);


    /**
     * @brief Salva le statistiche contenute nello snapshot su file CSV.
     * * Apre il file "stats.csv" in append. Se il file è vuoto, scrive l'header.
     * Scrive una riga contenente tutte le metriche raccolte nel DTO `DailyStatsSnapshot`.
     * * @param s Puntatore allo snapshot dei dati giornalieri già popolato.
     */
    void save_csv_report(DailyStatsSnapshot *s);

#endif // RESPONSABILE_UTILS_H
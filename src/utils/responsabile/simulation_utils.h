/**
 * @file simulation_utils.h
 * @brief Funzioni di supporto per la gestione logica della simulazione.
 * * Questo modulo isola la logica di business del Responsabile, gestendo
 * il refill giornaliero, l'inizializzazione delle strutture dati e
 * la generazione complessa dei processi (Operatori con priorità e Utenti).
 */

#ifndef SIMULATION_UTILS_H
    #define SIMULATION_UTILS_H

    // Includiamo common per conoscere la struttura GlobalState
    #include "../shared/common.h"

    /**
    * @brief Esegue il riempimento delle pentole all'inizio di ogni giorno.
    * * Accede alla memoria condivisa e ripristina le porzioni di Primi e Secondi
    * ai valori definiti in configurazione (AVG_REFILL).
    * Usa i mutex delle stazioni per garantire la coerenza, anche se a inizio giornata
    * la contesa è minima.
    */
    void perform_daily_refill();

    /**
    * @brief Inizializza lo stato globale della simulazione.
    * * Azzera le statistiche (incassi, utenti serviti), configura le stazioni
    * con le porzioni iniziali e imposta i valori iniziali di tutti i semafori.
    */
    void init_simulation_state();

    /**
    * @brief Genera i processi Operatori e Utenti applicando la logica di priorità.
    * * 1. Calcola la priorità delle stazioni basandosi su AVG_SRVC (i più lenti prima).
    * 2. Assegna gli operatori alle stazioni rispettando il limite dei posti a sedere.
    * 3. Esegue la spawn (fork/exec) degli operatori e degli utenti.
    * * @return int Il numero totale di processi figli generati (da usare per le wait).
    */
    int spawn_simulation_processes();

    /** (NON NECESSARIO - usata nello stesso file)
    * @brief Conta gli elementi separati da virgola nel file menu.
    * Esempio: "PRIMI=Pasta,Riso" -> Restituisce 2.
    * * @return int Il numero di elementi, del tipo (prefix) passato.
    */
    // int count_menu_items(const char *filename, const char *prefix);

    /**
    * @brief Resetta i contatori di sincronizzazione dei gruppi per l'inizio di un nuovo giorno.
    * * Questa funzione viene invocata dal Responsabile all'apertura della mensa (inizio ciclo for dei giorni).
    * Azzera l'array `group_ready_count` nello stato globale in memoria condivisa. 
    * * @details Il reset è fondamentale per la logica della "Barriera di Gruppo": senza di esso, 
    * i gruppi del Giorno 2 troverebbero i contatori già saturi dal Giorno 1, saltando 
    * l'attesa collettiva e violando il requisito di sincronizzazione.
    * * @note La funzione utilizza il semaforo SEM_STAT_MUTEX per garantire l'atomicità dell'operazione 
    * ed evitare race condition con utenti "extra" che potrebbero ancora essere in fase di uscita.
    */
    void reset_groups_for_new_day();

#endif // SIMULATION_UTILS_H
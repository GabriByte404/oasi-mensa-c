/**
 * @file operatore_utils.h
 * @brief Funzioni di supporto per il ciclo di vita dell'Operatore.
 * * Isola le logiche specifiche di Cassa (pagamenti) e Cucina (Refill),
 * oltre alla gestione delle pause e dell'inizializzazione.
 */

#ifndef OPERATORE_UTILS_H
#define OPERATORE_UTILS_H

#include "../shared/common.h"
#include <sys/types.h>

// Costante per l'indentazione dei log
#define OP_INDENT "\t\t\t\t\t" 

/**
 * @struct OpContext
 * @brief Raggruppa tutte le informazioni statiche dell'operatore.
 * Evita di passare decine di argomenti alle funzioni helper.
 */
typedef struct {
    StationType type;       /**< Tipo di stazione (PRIMI, CASSA, etc.) */
    int seat_sem;           /**< ID semaforo per la sedia */
    int mutex_id;           /**< ID semaforo mutex della stazione */
    char log_prefix[64];    /**< Stringa preformattata per i log (es. "[OP-PRIMI]") */
    int sem_id;             /**< ID SET semafori IPC */
    int msq_id;             /**< ID coda messaggi IPC */
} OpContext;

/**
 * @brief Inizializza il contesto dell'operatore in base agli argomenti.
 * * Configura il tipo, i semafori corretti e i colori per il log.
 * @param arg_type Stringa passata da argv[1] (es. "PRIMI").
 * @param ctx Puntatore alla struttura da riempire.
 * @return int 0 se successo, -1 se il tipo non è valido.
 */
int init_operator_context(const char *arg_type, OpContext *ctx);

/**
 * @brief Gestisce il lavoro della CASSA (lettura messaggi).
 * @param ctx Contesto operatore.
 * @param state Puntatore alla memoria condivisa.
 * @param ops_counter Puntatore al contatore operazioni (viene incrementato).
 */
void handle_cashier_task(OpContext *ctx, GlobalState *state, int *ops_counter);

/**
 * @brief Gestisce il lavoro della CUCINA (Controllo e Refill).
 * * Implementa la logica del Double Check Locking per il refill.
 * @param ctx Contesto operatore.
 * @param state Puntatore alla memoria condivisa.
 * @param ops_counter Puntatore al contatore operazioni.
 */
void handle_kitchen_task(OpContext *ctx, GlobalState *state, int *ops_counter);

/**
 * @brief Gestisce la logica della PAUSA.
 * * Controlla se l'operatore ha lavorato abbastanza e se ci sono le condizioni
 * per andare in pausa (colleghi attivi > 1).
 * @param ctx Contesto operatore.
 * @param state Puntatore alla memoria condivisa.
 * @param ops_counter Puntatore al contatore operazioni (viene resettato).
 * @param ops_threshold Soglia attuale per la pausa.
 * @param pauses_today Puntatore al contatore pause giornaliere.
 * @return int 1 se è andato in pausa, 0 altrimenti.
 */
int handle_break_logic(OpContext *ctx, GlobalState *state, int *ops_counter, int ops_threshold, int *pauses_today);

/**
 * @brief Genera un ritardo casuale in nanosecondi.
 * (Funzione di utilità generale, se non presente in time_utils).
 */
long get_random_delay_nanos(int avg_seconds, int percent_variation, long nanos_per_min);

#endif // OPERATORE_UTILS_H
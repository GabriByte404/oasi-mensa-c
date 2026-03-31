/**
 * @file utente_utils.h
 * @brief Funzioni di supporto per il ciclo di vita dell'Utente.
 * * Gestisce la lettura del menu, la logica di prelievo del cibo (con attese),
 * il pagamento e la consumazione al tavolo.
 */

#ifndef UTENTE_UTILS_H
#define UTENTE_UTILS_H

#include "../shared/common.h"

// File di configurazione del menu
#define MENU_FILE "./config/menu.txt"

/**
 * @struct MenuChoice
 * @brief Rappresenta le decisioni prese dall'utente leggendo il menu.
 */
typedef struct {
    int voglio_primo;    /**< 1 se l'utente desidera un primo. */
    int id_primo;        /**< ID del piatto scelto. */
    int voglio_secondo;  /**< 1 se l'utente desidera un secondo. */
    int id_secondo;      /**< ID del piatto scelto. */
    int voglio_coffee;   /**< 1 se l'utente desidera il caffè. */
    int id_coffee;       /**< ID del tipo di caffè. */
} MenuChoice;

/**
 * @brief Legge il file menu.txt e compila una scelta casuale.
 * * Se il file non esiste, genera una scelta completamente random.
 * @param scelta Puntatore alla struttura da riempire.
 * @param state Puntatore alla memoria condivisa in cui leggere il numero di piatti a disposizione.
 */
void menu_selection(MenuChoice *scelta, GlobalState *state);

/**
 * @brief Tenta di prelevare una porzione di cibo da una stazione.
 * * Gestisce l'accesso mutuamente esclusivo (semaforo), misura il tempo di attesa,
 * simula il servizio e decrementa le porzioni disponibili.
 * * @param sem_id ID del set di semafori.
 * @param state Puntatore alla memoria condivisa.
 * @param station_type Tipo di stazione (PRIMI, SECONDI, COFFEE).
 * @param dish_id ID del piatto desiderato.
 * @param tag Stringa identificativa dell'utente (per i log).
 * @return int 1 se il cibo è stato preso con successo, 0 altrimenti (es. finito).
 */
int take_food(int sem_id, GlobalState *state, int station_type, int dish_id, const char *tag);

/**
 * @brief Gestisce l'invio del messaggio di pagamento alla cassa.
 * @param msq_id ID della coda di messaggi.
 * @param user_pid PID dell'utente corrente.
 * @param n_primi Numero di primi presi.
 * @param n_secondi Numero di secondi presi.
 * @param n_coffee Numero di caffè presi.
 * @param tag Stringa identificativa dell'utente.
 * @param config Configurazione globale (per i prezzi).
 * @param has_ticket L'utente possiede un ticket
 */
void process_payment(int msq_id, pid_t user_pid, int n_primi, int n_secondi, int n_coffee, const char *tag, ConfigData config, int has_ticket);

/**
 * @brief Gestisce la fase di consumazione al tavolo.
 * * Attende un posto libero (semaforo tavoli), simula il tempo di pasto e rilascia il posto.
 * @param sem_id ID del set di semafori.
 * @param state Puntatore alla memoria condivisa (per i tempi in nanosecondi).
 * @param tag Stringa identificativa dell'utente.
 */
void consume_meal(int sem_id, GlobalState *state, const char *tag);

#endif // UTENTE_UTILS_H
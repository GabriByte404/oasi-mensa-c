/**
 * @file operatore.c
 * @brief Processo Lavoratore (Operatore Generico).
 * * Questo processo simula il comportamento dello staff della mensa.
 * A seconda degli argomenti passati, assume il ruolo di:
 * - Addetto ai Primi/Secondi/Caffè (Logica di cucina e refill).
 * - Cassiere (Logica di incasso pagamenti).
 * * Gestisce i turni di lavoro, le pause fisiologiche e la competizione
 * per le postazioni di lavoro limitate.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// --- HEADER PROGETTO ---
// Percorsi aggiornati alla nuova struttura a cartelle
#include "utils/shared/common.h"
#include "utils/shared/ipc_utils.h"
#include "utils/operatore/operatore_utils.h" // Modulo logica specifica operatore

/**
 * @brief Main del processo Operatore.
 * * Flusso di esecuzione:
 * 1. Setup contesto (tipo operatore, semafori specifici).
 * 2. Connessione alle IPC esistenti.
 * 3. Loop Principale (Simulazione):
 * - Competizione per la sedia (Inizio Turno).
 * - Loop Lavorativo (Svolgimento task ripetitivi).
 * - Gestione Pause (Rilascio sedia temporaneo).
 * - Rilascio risorse a fine giornata.
 */
int main(int argc, char *argv[]) {
    // Controllo argomenti (es. ./operatore PRIMI)
    if(argc < 2) exit(EXIT_FAILURE);

    // --- 1. SETUP INIZIALE ---
    OpContext ctx;
    // Inizializza la struttura in base all'argomento (es. colore, ID semafori)
    if (init_operator_context(argv[1], &ctx) != 0) {
        fprintf(stderr, "Tipo operatore sconosciuto: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // Connessione alle IPC (create dal Responsabile)
    GlobalState *state = connect_to_ipc(&ctx.sem_id, &ctx.msq_id);
    
    // Seed per la generazione casuale (PID ^ Time per unicità)
    srand(getpid() ^ time(NULL)); 

    printf(OP_INDENT "%s Pronto. Attendo start...\n", ctx.log_prefix);
    
    // Barriera di sincronizzazione iniziale
    sem_wait(ctx.sem_id, SEM_RESP_SYNC);

    // --- 2. INIT VARIABILI RUNTIME ---
    // Counter per simulare la "fatica" prima di una pausa
    int ops_counter = rand() % 15; 
    // Soglia variabile per richiedere la pausa
    int ops_before_break = 30 + rand() % 30; 
    
    int pauses_today = 0;
    int current_shift_day = 0; 

    // --- 3. MAIN LOOP (Ciclo di vita dell'operatore) ---
    while(1) {
        // Controllo globale fine simulazione
        if(state->current_day > state->config.SIM_DURATION) break;

        // --- COMPETIZIONE POSTO (INIZIO TURNO) ---
        // L'operatore cerca di sedersi. Se i posti sono finiti (semaforo a 0), attende.
        sem_wait(ctx.sem_id, ctx.seat_sem);
        
        // Segnalazione presenza (Statistiche)
        sem_wait(ctx.sem_id, SEM_STAT_MUTEX);
        state->stations[ctx.type].active_workers++;
        sem_signal(ctx.sem_id, SEM_STAT_MUTEX);

        // Rilevamento cambio giorno (reset contatori giornalieri)
        if (state->current_day > current_shift_day) {
            current_shift_day = state->current_day;
            pauses_today = 0;
            printf(OP_INDENT "%s Inizio turno " BOLD "Giorno %d" RESET ".\n", ctx.log_prefix, current_shift_day);
        }

        int seated = 1; // Flag per il loop lavorativo interno

        // --- LOOP LAVORATIVO (TURNO) ---
        while(seated) {
            
            // 1. Controllo Fine Giornata
            // Se il Responsabile ha avanzato il giorno mentre lavoravo, devo uscire.
            if (state->current_day > current_shift_day || state->current_day > state->config.SIM_DURATION) {
                seated = 0; 
                break; 
            }

            // 2. Svolgimento Compito Specifico
            if(ctx.type == CASSA) {
                // Logica Cassiere (Lettura messaggi)
                handle_cashier_task(&ctx, state, &ops_counter);
            } else {
                // Logica Cucina (Controllo scorte e Refill)
                handle_kitchen_task(&ctx, state, &ops_counter);
            }

            // 3. Gestione Pausa
            // handle_break_logic restituisce 1 se l'operatore è andato in pausa.
            // In tal caso, la funzione ha già rilasciato la sedia e decrementato active_workers.
            if (handle_break_logic(&ctx, state, &ops_counter, ops_before_break, &pauses_today)) {
                
                // Sono tornato dalla pausa: devo uscire dal loop 'seated' per 
                // tornare all'inizio del main loop e ricompetere per la sedia (sem_wait).
                seated = 0; 
                
                // Ricalcolo nuova soglia fatica per il prossimo turno
                ops_before_break = 30 + rand() % 30; 
            }

        }

        // --- USCITA DAL TURNO (Fine Giorno o Rientro da Pausa) ---
        
        // Caso A: Uscita per Fine Giornata (Responsabile ha cambiato giorno)
        // Dobbiamo rilasciare la sedia e aggiornare le statistiche manualmente.
        if (state->current_day > current_shift_day || state->current_day > state->config.SIM_DURATION) {
             
            // Decremento lavoratori attivi
            sem_wait(ctx.sem_id, SEM_STAT_MUTEX);
            state->stations[ctx.type].active_workers--;
            sem_signal(ctx.sem_id, SEM_STAT_MUTEX);
            
            // Rilascio il posto (Vado a casa)
            sem_signal(ctx.sem_id, ctx.seat_sem);
            
            // Aggiorno il giorno locale per sincronizzarmi
            current_shift_day = state->current_day;
            
            // Piccola pausa per evitare race condition nel riprendere il semaforo istantaneamente
            usleep(10000); 
        }
        
        // Caso B: Uscita per Pausa
        // Non facciamo nulla qui, perché handle_break_logic ha già rilasciato sedia e statistiche.
    }

    // Detach della memoria (opzionale, l'OS pulisce all'exit, ma buona norma)
    shmdt(state);
    return 0;
}
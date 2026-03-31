#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/msg.h>

#include "operatore_utils.h"
#include "../shared/ipc_utils.h"

// --- IMPLEMENTAZIONE FUNZIONI ---

long get_random_delay_nanos(int avg_seconds, int percent_variation, long nanos_per_min) {
    if (avg_seconds <= 0) return 0;
    double variation = (avg_seconds * percent_variation) / 100.0;
    double min_sec = avg_seconds - variation;
    double max_sec = avg_seconds + variation;
    // ((double)rand() / RAND_MAX) -> numero tra 0.0 e 1.0
    double random_sec = min_sec + ((double)rand() / RAND_MAX) * (max_sec - min_sec);
    double nanos_per_sec = (double)nanos_per_min / 60.0;
    return (long)(random_sec * nanos_per_sec);
}

int init_operator_context(const char *arg_type, OpContext *ctx) {
    char type_str[10];
    char *color_op;

    if(strcmp(arg_type, "PRIMI")==0) { 
        ctx->type=PRIMI; ctx->seat_sem=SEM_WK_PRIMI_SEATS; ctx->mutex_id=SEM_PRIMI_MUTEX; 
        strcpy(type_str, "PRIMI"); color_op=BLUE; 
    }
    else if(strcmp(arg_type, "SECONDI")==0) { 
        ctx->type=SECONDI; ctx->seat_sem=SEM_WK_SECONDI_SEATS; ctx->mutex_id=SEM_SECONDI_MUTEX; 
        strcpy(type_str, "SECONDI"); color_op=BLUE; 
    }
    else if(strcmp(arg_type, "COFFEE")==0) { 
        ctx->type=COFFEE; ctx->seat_sem=SEM_WK_COFFEE_SEATS; ctx->mutex_id=SEM_COFFEE_MUTEX; 
        strcpy(type_str, "CAFFE"); color_op=BLUE; 
    }
    else if(strcmp(arg_type, "CASSA")==0) { 
        ctx->type=CASSA; ctx->seat_sem=SEM_WK_CASSA_SEATS; ctx->mutex_id=SEM_CASSA_MUTEX; 
        strcpy(type_str, "CASSA"); color_op=GREEN; 
    }
    else return -1;

    sprintf(ctx->log_prefix, "%s[OP-%-7s]" RESET, color_op, type_str);
    return 0;
}

void handle_cashier_task(OpContext *ctx, GlobalState *state, int *ops_counter) {
    PaymentMessage msg;
    // Ricezione non bloccante
    if(msgrcv(ctx->msq_id, &msg, sizeof(PaymentMessage)-sizeof(long), 1, 0) != -1) {
        
        // 1. BLOCCO DISORDER (Qui ci si ferma se c'è guasto)
        sem_wait(ctx->sem_id, ctx->mutex_id);

        // 2. Lavoro (Aggiorno statistiche, sleep simulata, incasso)
        // Aggiornamento statistiche (Protetto)
        sem_wait(ctx->sem_id, SEM_STAT_MUTEX);
        state->total_revenue += msg.total_to_pay;
        sem_signal(ctx->sem_id, SEM_STAT_MUTEX);
        
        // Simulazione tempo servizio
        long wait_nanos = get_random_delay_nanos(state->config.AVG_SRVC_CASSA, 20, state->config.N_NANO_SECS);
        struct timespec ts = {wait_nanos/1000000000L, wait_nanos%1000000000L};
        nanosleep(&ts, NULL);

        // 3. RILASCIO MUTEX CASSA (Finito il lavoro critico)
        sem_signal(ctx->sem_id, ctx->mutex_id);
        
        // INVIO SCONTRINO (Sblocco l'utente)
        // Uso msg.user_pid come mtype per indirizzare il messaggio SOLO a lui
        msg.mtype = msg.user_pid; 
        // Invio conferma (non serve cambiare il contenuto, basta il segnale)
        msgsnd(ctx->msq_id, &msg, sizeof(PaymentMessage)-sizeof(long), 0);
        
        printf(OP_INDENT "%s Incasso %.2f € da User %d (Scontrino inviato)\n", ctx->log_prefix, msg.total_to_pay, msg.user_pid);
        (*ops_counter)++;

    } else {
        // Se arriva qui con -1, solitamente è perché il processo è stato interrotto 
        // da un segnale (es. spegnimento) o la coda è stata rimossa.
        // È il momento giusto per uscire o gestire l'errore.
        perror("Errore ricezione cassa");
    }
}

void handle_kitchen_task(OpContext *ctx, GlobalState *state, int *ops_counter) {
    int need_refill = 0;
    
    // --- INIZIO ZONA CRITICA ESTESA ---
    // 1. Prendo il Mutex SUBITO. Nessuno (né utenti, né altri operatori)
    //    potrà toccare questa stazione finché non faccio la signal in fondo.
    sem_wait(ctx->sem_id, ctx->mutex_id); 

    // 2. Controllo le scorte
    for(int i=0; i<MAX_DISHES_PER_TYPE; i++) {
        int p = state->stations[ctx->type].dish_portions[i];
        // Ignora caffè e slot non usati
        if(ctx->type != COFFEE && p != -1) {
             if (p < state->config.REFILL_THRESHOLD) {
                need_refill = 1;
                break;
             }
        }
    }

    // 3. Se serve Refill, CUCINO (Mantenendo il blocco!)
    if(need_refill) {
        printf(OP_INDENT "%s " RED "SCORTE BASSE" RESET ". Blocco linea e cucino...\n", ctx->log_prefix);
        
        // A. Simulo il tempo di cottura
        long refill_nanos = get_random_delay_nanos(state->config.AVG_REFILL_TIME, 20, state->config.N_NANO_SECS);
        struct timespec ts = {refill_nanos/1000000000L, refill_nanos%1000000000L};
        nanosleep(&ts, NULL);
        
        // --- B. Riempio SOLO le vaschette vuote (Con logica Incrementale) ---
        for(int i=0; i<MAX_DISHES_PER_TYPE; i++) {
            
            // 1. Lo slot esiste?
            if(state->stations[ctx->type].dish_portions[i] != -1) {
                
                // 2. È sotto la soglia minima?
                if (state->stations[ctx->type].dish_portions[i] < state->config.REFILL_THRESHOLD) {
                    
                    // A. Determino il Tetto Massimo per questo tipo di piatto
                    int max_cap;
                    if (ctx->type == PRIMI) {
                        max_cap = state->config.MAX_PORZIONI_PRIMI;
                    } else {
                        max_cap = state->config.MAX_PORZIONI_SECONDI;
                    }

                    // B. Calcolo la nuova quantità
                    int current_level = state->stations[ctx->type].dish_portions[i];
                    int new_level = current_level + state->config.REFILL_AMOUNT;

                    // C. Controllo anti-traboccamento (Clamping)
                    if (new_level > max_cap) {
                        new_level = max_cap; // Taglio l'eccedenza
                    }

                    // D. Scrivo in memoria
                    state->stations[ctx->type].dish_portions[i] = new_level;
                }
            }
        }
        
        printf(OP_INDENT "%s " GREEN "Refill Completato" RESET ". Riapro la linea.\n", ctx->log_prefix);
        *ops_counter += 5; // Fatica maggiore per il refill

    } 
    
    // --- FINE ZONA CRITICA ---
    // 4. Rilascio il Mutex solo ORA. 
    // Tutti gli utenti che si erano accumulati in fila possono finalmente servirsi.
    sem_signal(ctx->sem_id, ctx->mutex_id);


    // 5. Gestione Idle
    // Se non ho dovuto cucinare, riposo un attimo per non stressare la CPU.
    if(!need_refill) {
        usleep(50000); 
        (*ops_counter)++;
    }
}

int handle_break_logic(OpContext *ctx, GlobalState *state, int *ops_counter, int ops_threshold, int *pauses_today) {
    // Se non ho raggiunto la soglia di fatica, esco
    if (*ops_counter < ops_threshold) return 0;

    // Probabilità casuale (20%) per non uscire tutti insieme
    if ((rand() % 100) >= 20) {
        // Ho deciso di lavorare ancora un po', riduco counter per riprovare dopo
        *ops_counter -= 5;
        return 0;
    }

    // Controllo Condizioni Critiche
    sem_wait(ctx->sem_id, SEM_STAT_MUTEX);
    
    if ((state->stations[ctx->type].active_workers > 1) && (*pauses_today < state->config.NOF_PAUSES)) {
        state->stations[ctx->type].active_workers--;
        state->total_pauses++; 
        sem_signal(ctx->sem_id, SEM_STAT_MUTEX);
        
        // Rilascio la sedia (Esco dalla zona lavoro)
        sem_signal(ctx->sem_id, ctx->seat_sem);

        long pause_ns = 2 * state->config.N_NANO_SECS; // Pausa di 2 minuti simulati
        struct timespec ts_pause = {pause_ns/1000000000L, pause_ns%1000000000L};

        printf(OP_INDENT "%s " YELLOW "Vado in pausa" RESET " (%d/%d)...\n", 
            ctx->log_prefix, *pauses_today + 1, state->config.NOF_PAUSES);
        
        nanosleep(&ts_pause, NULL);
        
        (*pauses_today)++;
        *ops_counter = 0; // Reset fatica
        
        printf(OP_INDENT "%s " YELLOW "Pausa finita." RESET " Rientro...\n", ctx->log_prefix);
        return 1; // Sono andato in pausa
    } else {
        sem_signal(ctx->sem_id, SEM_STAT_MUTEX);
        // Non potevo andare (troppo pochi colleghi). Riprovo dopo.
        *ops_counter -= 10; 
        return 0;
    }
}
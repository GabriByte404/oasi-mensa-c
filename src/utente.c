/**
 * @file utente.c
 * @brief Processo Client (Utente della Mensa).
 * * Il ciclo prevede:
 * 1. Attesa apertura (Polling sul cambio giorno).
 * 2. Scelta del menu.
 * 3. Prelievo cibo, Pagamento, Consumazione.
 * 4. Loop finché la simulazione non termina.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

// --- HEADER PROGETTO ---
#include "utils/shared/common.h"
#include "utils/shared/ipc_utils.h"
#include "utils/utente/utente_utils.h" 

int main(int argc, char *argv[]) {

    // --- 1. SETUP INIZIALE ---
    int sem_id, msq_id; 
    GlobalState *state = connect_to_ipc(&sem_id, &msq_id);
    
    // Leggo argomenti: argv[1] = group_id, argv[2] = "extra"
    int group_id;
    if (argc > 1) {
        group_id = atoi(argv[1]);
    } else {
        group_id = 0;
    }

    int is_extra;
    if (argc > 2 && strcmp(argv[2], "extra") == 0) {
        is_extra = 1;
    } else {
        is_extra = 0;
    }

    // Registrazione PID se extra
    if (is_extra) {
        sem_wait(sem_id, SEM_STAT_MUTEX);
        if (state->extra_users_count < MAX_EXTRA_USERS) {
            state->extra_users_pids[state->extra_users_count++] = getpid();
        }
        sem_signal(sem_id, SEM_STAT_MUTEX);
    }

    int has_ticket;
    srand(getpid() ^ time(NULL));
    
    if((rand() % 100) < 80){
        has_ticket = 1;
    } else{
        has_ticket = 0;
    }

    // Creazione Tag Log
    char tag[30];
    sprintf(tag, CYAN "[USER %-5d]" RESET, getpid());

    printf("%s Pronto. Attendo start..." BOLD "[G_ID = %d]\n" RESET, tag, group_id);

    // SE NON SONO UN EXTRA, aspetto lo start del Responsabile
    // SE SONO EXTRA, entro subito perché la simulazione è già in corso
    if (!is_extra) {
        sem_wait(sem_id, SEM_RESP_SYNC); 
    }
    
    printf("%s Accesso effettuato.\n", tag);

    // Variabile locale per tracciare i giorni vissuti
    int last_day = 0;

    // --- 2. LOOP GIORNALIERO ---
    while(1) {
        // Controllo Fine Simulazione
        int current_day_server = state->current_day;
        
        if (current_day_server > state->config.SIM_DURATION) {
            break; // Esci dal ciclo e termina
        }

        // SE È INIZIATO UN NUOVO GIORNO
        if (current_day_server > last_day) {
            
            // Aggiorno SUBITO il giorno corrente per evitare di rieseguire se il ciclo è veloce
            last_day = current_day_server;

            // --- A. ARRIVO SCAGLIONATO ---
            // Arrivo casuale entro 360 minuti (metà giornata lavorativa)
            int arrival_delay_min = rand() % 360; 
            long delay_ns = (long)arrival_delay_min * state->config.N_NANO_SECS;
            struct timespec ts_arrival = {delay_ns / 1000000000L, delay_ns % 1000000000L};
            
            // Simula attesa prima di entrare in mensa
            nanosleep(&ts_arrival, NULL);
            
            printf("%s " BOLD "Giorno %d" RESET ": Entro in mensa.\n", tag, last_day);
            
            // --- B. SCELTA MENU ---
            MenuChoice choice;
            // Passiamo 'state' perché la funzione deve leggere i limiti del menu dalla SHM
            menu_selection(&choice, state); 
            
            int n_primi = 0, n_secondi = 0, n_coffee = 0;
            int collected_dishes = 0;

            // --- C. PRELIEVO CIBO ---
            // Primo
            if (choice.voglio_primo) {
                if (take_food(sem_id, state, PRIMI, choice.id_primo, tag)) {
                    n_primi++; collected_dishes++;
                }
            }
            // Secondo
            if (choice.voglio_secondo) {
                if (take_food(sem_id, state, SECONDI, choice.id_secondo, tag)) {
                    n_secondi++; collected_dishes++;
                }
            }
            // Caffè
            if (choice.voglio_coffee) {
                if (take_food(sem_id, state, COFFEE, choice.id_coffee, tag)) {
                    n_coffee++; 
                }
            }

            // --- LOGICA GRUPPI (Barriera) ---
            if (group_id > 0) {
                sem_wait(sem_id, SEM_STAT_MUTEX);
                state->group_ready_count[group_id]++;
                
                int pronti = state->group_ready_count[group_id];
                int totali = state->group_members_count[group_id];
                
                int am_i_last = (pronti == totali);
                sem_signal(sem_id, SEM_STAT_MUTEX);

                if (am_i_last) {
                    // Log dell'ultimo arrivato che sblocca il gruppo
                    printf("%s " GREEN"Gruppo %-3d: %d/%d pronti. GRUPPO COMPLETO! Sblocco i compagni e vado in cassa." RESET "\n", 
                        tag, group_id, pronti, totali);
                    
                    for(int k = 0; k < totali - 1; k++) {
                        sem_signal(sem_id, SEM_GROUP_BARRIER);
                    }
                } else {
                    // Log di chi si mette in attesa
                    printf("%s " YELLOW "Gruppo %-3d: [⋯] %d/%d pronti. In attesa degli altri membri..." RESET "\n", tag, group_id, pronti, totali);
                    
                    sem_wait(sem_id, SEM_GROUP_BARRIER);
                    
                }
            }

            // --- LOGICA TICKET (Attesa 2-5 minuti simulati) ---
            if (has_ticket) {
                long min_ns = 2L * state->config.N_NANO_SECS;
                long max_ns = 5L * state->config.N_NANO_SECS;

                // 2. Generiamo un ritardo casuale nell'intervallo [min, max]
                long delay_ns = min_ns + (rand() % (max_ns - min_ns + 1));

                printf("%s " YELLOW "Presento il ticket. Validazione in corso (Attesa: %.1f min simulati)..." RESET "\n", tag, (double)delay_ns / state->config.N_NANO_SECS);

                // 3. Eseguiamo l'attesa. 
                // Siccome usleep usa i microsecondi, dividiamo i nanosecondi per 1000.
                usleep(delay_ns / 1000); 
            }

            // --- D. ESITO ---
            if (collected_dishes == 0 && n_coffee == 0) {
                // Caso: Non ho preso nulla (nemmeno un caffè)
                printf("%s " RED "Nessun cibo disponibile oggi!" RESET " (Digiuno)\n", tag);
                
                sem_wait(sem_id, SEM_STAT_MUTEX);
                state->unserved_users_total++;
                sem_signal(sem_id, SEM_STAT_MUTEX);
            } 
            else {
                // Caso: Ho preso qualcosa -> Pago e Mangio
                
                // Pagamento (MsgQueue verso Cassiere)
                process_payment(msq_id, getpid(), n_primi, n_secondi, n_coffee, tag, state->config, has_ticket);
                
                // Consumazione (Sleep temporizzata e occupazione tavolo)
                consume_meal(sem_id, state, tag);
                
                // Statistiche successo
                sem_wait(sem_id, SEM_STAT_MUTEX);
                state->served_users_total++;
                sem_signal(sem_id, SEM_STAT_MUTEX);
                
                printf("%s Finito giorno %d. Esco.\n", tag, last_day);
            }
        }

        // --- ATTESA ATTIVA OTTIMIZZATA ---
        // Dormiamo un po' per non fondere la CPU mentre aspettiamo il giorno dopo.
        usleep(50000); 
    }

    // Uscita pulita
    shmdt(state);
    printf("%s Terminazione processo.\n", tag);
    return 0;
}
/**
 * @file responsabile.c
 * @brief Processo Master (Orchestratore) della simulazione Mensa.
 * * Questo processo è il punto di ingresso dell'applicazione. Si occupa di:
 * 1. Creare e inizializzare le risorse IPC (Memoria Condivisa, Semafori, Coda Messaggi).
 * 2. Caricare la configurazione da file.
 * 3. Lanciare i processi figli (Operatori e Utenti) delegando la logica complessa ai moduli helper.
 * 4. Gestire il ciclo temporale della simulazione (scorrere dei giorni e refill).
 * 5. Gestire la terminazione pulita e il rilascio delle risorse.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/sem.h>

// --- HEADER PROGETTO ---
#include "utils/shared/common.h"
#include "utils/shared/ipc_utils.h"
#include "utils/shared/config.h"
#include "utils/responsabile/report_utils.h"     // Per print_daily_report
#include "utils/responsabile/process_utils.h"    // (Necessario per allocazione pid)
#include "utils/responsabile/simulation_utils.h" // Per init, spawn, refill (Business Logic)

// --- VARIABILI GLOBALI ---
// Necessarie globali per essere accessibili dal Signal Handler in caso di CTRL+C
// e dai moduli esterni (simulation_utils) tramite 'extern'.
int shm_id = -1;            /**< ID del segmento di Memoria Condivisa. */
int sem_id = -1;            /**< ID del set di Semafori. */
int msq_id = -1;            /**< ID della Coda di Messaggi. */
pid_t *child_pids = NULL;   /**< Array dinamico per tracciare i PID dei processi figli. */
int total_procs_count = 0;  /**< Numero totale di processi figli lanciati. */
GlobalState *state = NULL;  /**< Puntatore alla struttura di stato in Memoria Condivisa. */

/**
 * @brief Gestore dei segnali di terminazione (SIGINT) e chiusura ordinaria.
 * * Questa funzione garantisce che, sia in caso di fine simulazione naturale che
 * di interruzione forzata (CTRL+C), tutte le risorse vengano rilasciate correttamente
 * e nessun processo "zombie" o "orfano" rimanga attivo.
 * * @param sig Il segnale ricevuto. Se 0, indica una chiusura ordinaria richiamata dal main.
 */
void handle_sigint(int sig) {
    // Distinzione tra uscita normale (0) e segnale di interruzione
    if (sig == 0) {
        printf("\n" GREEN "=== [RESP] SIMULAZIONE COMPLETATA CON SUCCESSO ===" RESET "\n");
        printf(GREEN "[RESP] Avvio procedura di chiusura ordinaria..." RESET "\n");
    } else {
        printf("\n\n" RED "=== [RESP] RICEVUTO CTRL+C (Segnale %d) ===" RESET "\n", sig);
        printf(RED "[RESP] Terminazione forzata della simulazione..." RESET "\n");
    }

    // 1. Terminazione dei processi figli
    if (child_pids != NULL) {
        if (sig != 0) {
            printf("[RESP] Invio SIGKILL a %d processi figli...\n", total_procs_count);
        }
        
        // Invio segnale di kill a tutti i PID registrati
        for (int i = 0; i < total_procs_count; i++) {
            if (child_pids[i] > 0) {
                kill(child_pids[i], SIGKILL);
            }
        }

        // Uccido gli utenti extra registrati in SHM
        if (state != NULL) {
            for (int i = 0; i < state->extra_users_count; i++) {
                if (state->extra_users_pids[i] > 0) {
                    kill(state->extra_users_pids[i], SIGKILL);
                }
            }
        }
        
        // Wait non bloccante per pulire la tabella dei processi (evita zombie)
        // Usiamo WNOHANG per non bloccare il responsabile se un figlio è lento a morire
        for (int i = 0; i < total_procs_count; i++) {
            if (child_pids[i] > 0) waitpid(child_pids[i], NULL, WNOHANG);
        }
        
        if (sig != 0) printf("[RESP] Figli terminati.\n");
        free(child_pids);
    }

    // 2. Detach della Memoria Condivisa
    // Importante staccarsi prima di distruggere, anche se l'OS lo farebbe comunque all'exit
    if (state != NULL) {
        shmdt(state);
    }

    // 3. Rimozione completa risorse IPC
    // Questa è la parte critica: IPC_RMID deve essere chiamato per non lasciare risorse appese
    printf("[RESP] Pulizia risorse IPC...\n");
    cleanup_ipc_resources(shm_id, sem_id, msq_id);

    printf(GREEN "[RESP] Bye!" RESET "\n");
    exit(0);
}

/**
 * @brief Main del processo Responsabile.
 * * Flusso di esecuzione:
 * 1. Setup segnali e IPC.
 * 2. Init configurazione e stato (delegato a helper).
 * 3. Spawn processi Operatori e Utenti (delegato a helper).
 * 4. Loop temporale (Giorni -> Minuti) con Refill giornaliero.
 * 5. Reportistica e pulizia finale.
 */
int main(int argc, char *argv[]) {
    
    printf("=== RESPONSABILE: Avvio simulazione Mensa ===\n");

    // --- 0. REGISTRAZIONE SIGNAL HANDLER ---
    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        perror("Errore nella registrazione del segnale");
        exit(EXIT_FAILURE);
    }

    // --- 1. SETUP AMBIENTE IPC ---
    
    // Creazione file anchor per ftok (se non esiste)
    // Necessario perché ftok richiede un file esistente nel filesystem
    FILE *fp = fopen(IPC_KEY_FILE_PATH, "w");
    if (fp == NULL) {
        perror("Errore creazione file anchor");
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    printf("Responsabile: File anchor '%s' verificato.\n", IPC_KEY_FILE_PATH);
    
    // Creazione risorse IPC (System V)
    shm_id = create_shared_memory(sizeof(GlobalState));
    printf("Responsabile: Memoria Condivisa creata (ID: %d)\n", shm_id);

    sem_id = create_semaphore_set(TOTAL_SEMAPHORES);
    printf("Responsabile: Set di Semafori creato (ID: %d)\n", sem_id);

    msq_id = create_message_queue();
    printf("Responsabile: Coda di Messaggi creata (ID: %d)\n", msq_id);

    // Attach della memoria (Mapping indirizzo logico -> fisico)
    state = (GlobalState *) attach_shared_memory(shm_id);

    // --- 2. CARICAMENTO CONFIGURAZIONE ---
    printf("Responsabile: Caricamento configurazione...\n");
    // Il parser legge riga per riga e popola la struct ConfigData
    if (load_config("./config/config_timeout.conf", &state->config) == -1) {
        fprintf(stderr, RED "Errore: Impossibile caricare la configurazione!" RESET "\n");
        // Pulizia immediata prima di uscire per errore
        handle_sigint(1); 
    }

    // Stampa di debug parametri critici
    printf("--- CONFIGURAZIONE ATTIVA ---\n");
    printf("Durata Simulazione: %d giorni\n", state->config.SIM_DURATION);
    printf("Numero Utenti:      %d\n", state->config.NOF_USERS);
    printf("Posti Tavoli:       %d\n", state->config.NOF_TABLE_SEATS);
    printf("-----------------------------\n");

    // --- 3. INIZIALIZZAZIONE STATO GLOBALE (Helper) ---
    // Deleghiamo al modulo 'simulation_utils' tutta la logica di setup:
    // 1. Azzera statistiche.
    // 2. Analizza 'menu.txt' per contare i piatti.
    // 3. Inizializza l'array delle stazioni.
    // 4. Configura i valori iniziali dei semafori (Mutex e Posti).
    init_simulation_state(); 

    // --- 4. GENERAZIONE PROCESSI (Helper) ---
    
    // Allocazione array PID per la gestione
    // Deve essere fatto qui nel main per poterlo liberare in handle_sigint
    int max_procs = state->config.NOF_WORKERS + state->config.NOF_USERS;
    child_pids = malloc(sizeof(pid_t) * max_procs);
    if (!child_pids) { 
        perror("Malloc pids"); 
        handle_sigint(1); // Uscita pulita in caso di errore di memoria
    }

    // Deleghiamo la creazione complessa al modulo 'simulation_utils'.
    // Si occupa di: Bubble Sort priorità operatori, assegnazione sedie, e fork().
    total_procs_count = spawn_simulation_processes();

    // --- 5. START SIMULAZIONE ---
    printf("Responsabile: Attendo assestamento processi (2s)...\n");
    sleep(2); 

    printf("Responsabile: VIA! Sblocco barriera per %d processi.\n", total_procs_count);
    // Sblocco tutti i processi (Operatori e Utenti) in attesa su SEM_RESP_SYNC
    for(int i = 0; i < total_procs_count; i++){
        sem_signal(sem_id, SEM_RESP_SYNC);
    }
    
    // --- 6. CICLO TEMPORALE (SIMULATION LOOP) ---
    struct timespec minute_delay;
    minute_delay.tv_sec = 0;
    minute_delay.tv_nsec = state->config.N_NANO_SECS; // Configurazione velocità tempo
    int minutes_per_day = 480; // 8 ore lavorative simulate

    for(int day = 0; day < state->config.SIM_DURATION; day++){
        state->current_day = day + 1;
        printf("\n" MAGENTA "-------------------------------------" RESET "\n");
        printf(MAGENTA "Responsabile: Inizia il giorno %d..." RESET "\n", state->current_day);
        printf(MAGENTA "-------------------------------------" RESET "\n");

        // --- REFILL MATTUTINO & RESET SINCRO GRUPPI (Helper) ---
        if (day > 0) {
            perform_daily_refill();
            reset_groups_for_new_day();
        }
        
        // --- SCORRERE DEL TEMPO ---
        // Il responsabile "detta il tempo" dormendo per ogni minuto simulato.
        // Questo ciclo scandisce la durata della giornata lavorativa.
        for(int min = 0; min < minutes_per_day; min++){
            nanosleep(&minute_delay, NULL);
        }
        
        // --- REPORT GIORNALIERO ---
        // Stampa le statistiche di fine giornata (incassi, piatti serviti, sprechi).
        print_daily_report(day, state, sem_id);

        // 1. Contiamo quanti utenti sono bloccati in coda ai semafori
        int waiting_users = 0;

        // Coda ai PRIMI (Gente che aspetta il lock per prendere cibo)
        waiting_users += semctl(sem_id, SEM_PRIMI_MUTEX, GETNCNT);
        
        // Coda ai SECONDI
        waiting_users += semctl(sem_id, SEM_SECONDI_MUTEX, GETNCNT);

        // Coda ai COFFEE
        waiting_users += semctl(sem_id, SEM_COFFEE_MUTEX, GETNCNT);
        
        // Coda alla CASSA (Gente che aspetta di pagare)
        // Dichiarazione della struct per le info sulla coda
        struct msqid_ds queue_info;

        // Ottengo le statistiche della coda
        msgctl(msq_id, IPC_STAT, &queue_info);
        waiting_users += queue_info.msg_qnum;
        waiting_users += semctl(sem_id, SEM_CASSA_MUTEX, GETNCNT);

        // Coda ai TAVOLI (Gente col vassoio in mano che aspetta di sedersi)
        waiting_users += semctl(sem_id, SEM_TABLE_SEATS, GETNCNT);

        printf("[RESP] Controllo Overload: %d utenti in attesa (Soglia: %d)\n", waiting_users, state->config.OVERLOAD_THRESHOLD);

        // 2. Verifica della condizione di terminazione
        if (waiting_users > state->config.OVERLOAD_THRESHOLD) {
            printf("\n" RED "!!! TERMINAZIONE ANTICIPATA: OVERLOAD !!!" RESET "\n");
            printf(RED "Causa: Troppi utenti in attesa (%d > %d) al termine del Giorno %d." RESET "\n", waiting_users, state->config.OVERLOAD_THRESHOLD, state->current_day);
            
            // Usciamo dal ciclo dei giorni --> Si va alla pulizia finale
            break; 
        }
    }

    // --- 7. CONCLUSIONE ---
    printf("\n=== FINE SIMULAZIONE ===\n");
    
    // Chiamata esplicita al gestore per pulizia e chiusura ordinaria
    handle_sigint(0);

    return 0; // Mai raggiunto grazie a exit(0) in handle_sigint
}
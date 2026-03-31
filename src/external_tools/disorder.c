#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h> 
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "../utils/shared/common.h"
#include "../utils/shared/ipc_utils.h"

// Variabile globale per il Signal Handler
int g_sem_id = -1;

// Gestore CTRL+C
void handle_sigint(int sig) {
    printf("\n" YELLOW "[DISORDER] Interruzione forzata! Rilascio la cassa..." RESET "\n");
    if (g_sem_id != -1) {
        sem_signal(g_sem_id, SEM_CASSA_MUTEX);
    }
    printf(RED "[DISORDER] Terminato.\n" RESET);
    exit(0);
}

int main(int argc, char *argv[]){
    // 1. GESTIONE SEGNALI
    signal(SIGINT, handle_sigint);

    // 2. CONTROLLI INPUT
    if(argc < 2){
        printf("Uso: '%s <SECONDI_REALI>'\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int input_real_seconds = atoi(argv[1]);
    if(input_real_seconds <= 0){
        printf("Durata non valida! (%d)\n", input_real_seconds);
        exit(EXIT_FAILURE);
    }

    // 3. CONNESSIONE IPC (serve solo il semaforo)
    int msq_id;
    GlobalState *state = connect_to_ipc(&g_sem_id, &msq_id);
    
    if (state == NULL) {
        fprintf(stderr, RED "Errore connessione. Mensa chiusa?\n" RESET);
        exit(EXIT_FAILURE);
    }

    // 4. CALCOLO INFO
    long nanos_per_sim_minute = state->config.N_NANO_SECS;
    long input_total_nanos = (long)input_real_seconds * 1000000000L;
    long equivalent_sim_minutes = input_total_nanos / nanos_per_sim_minute;

    printf(BLUE "[DISORDER]" RESET " Connesso. ID Semafori: %d\n", g_sem_id);
    printf("[DISORDER] Input: %d sec reali (corrispondono a ~%ld min simulati)\n",  input_real_seconds, equivalent_sim_minutes);

    // 5. SABOTAGGIO
    printf("[DISORDER] Tento di bloccare la cassa (attendo se occupata)...\n");
    
    // Tenta di acquisire il semaforo.
    sem_wait(g_sem_id, SEM_CASSA_MUTEX);

    printf(BOLD "\t[DISORDER] GUASTO IN CORSO... (%d sec) " RESET "\n", input_real_seconds);

    // Dormo per tutto il tempo richiesto (NESSUN LIMITE)
    sleep(input_real_seconds);

    // 6. RIPRISTINO
    printf("[DISORDER] Ripristino cassa...\n");
    // Usiamo semop direttamente invece di sem_signal per gestire l'errore "Semaforo rimosso"
    struct sembuf sops = {SEM_CASSA_MUTEX, 1, 0}; // 1 = Signal (V)
    
    if (semop(g_sem_id, &sops, 1) == -1) {
        // Se la semop fallisce, controlliamo il motivo in 'errno'
        if (errno == EINVAL || errno == EIDRM) {
            // EINVAL = Invalid Argument (L'ID non esiste più)
            // EIDRM = ID Removed (Il semaforo è stato rimosso)
            printf(YELLOW "\n[DISORDER] Nota: La mensa ha chiuso (e rimosso i semafori) mentre ero attivo.\n" RESET);
            printf(GREEN "[DISORDER] Termino senza errori.\n" RESET);
        } else {
            // Se è un altro errore, allora è un problema vero
            perror("sem_signal error");
            exit(EXIT_FAILURE);
        }
    } else {
        printf(GREEN "[DISORDER] Guasto Terminato!\n" RESET);
    }

    return 0;
}
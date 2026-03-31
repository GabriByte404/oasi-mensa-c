/**
 * @file ipc_utils.h
 * @brief Libreria di utilità per la gestione delle IPC (Inter-Process Communication).
 * * Questo modulo fornisce funzioni wrapper semplificate per gestire le risorse
 * System V: Memoria Condivisa, Semafori e Code di Messaggi.
 * Le funzioni gestiscono internamente il controllo degli errori, terminando
 * il processo in caso di fallimento delle system call.
 */

#ifndef IPC_UTILS_H
    #define IPC_UTILS_H

    #include <stdio.h>      // Per printf, perror, fprintf
    #include <stdlib.h>     // Per exit, EXIT_FAILURE, NULL
    #include <sys/types.h>  // Per definizioni di tipi di sistema (key_t, pid_t)
    #include <sys/ipc.h>    // Per flag IPC (IPC_CREAT, IPC_EXCL, IPC_RMID) e ftok
    #include <sys/shm.h>    // Per shmget, shmat, shmctl
    #include <sys/sem.h>    // Per semget, semop, semctl, struct sembuf
    #include <sys/msg.h>    // Per msgget, msgctl
    #include <errno.h>      // Per la gestione degli errori (errno)
    #include "./common.h"   // Per la definizione di GlobalState

    /**
    * @brief Genera una chiave IPC univoca utilizzando ftok.
    * * @param path Percorso del file esistente da usare come ancora (es. "./ipc_key_anchor").
    * @param proj_id Identificativo del progetto (un carattere, es. 'A').
    * @return key_t La chiave generata.
    * @note Termina il programma se ftok fallisce.
    */
    key_t get_ipc_key(const char *path, int proj_id);

    /**
    * @brief Crea un segmento di memoria condivisa.
    * * Utilizza `shmget` con flag IPC_CREAT per allocare un nuovo segmento.
    * @param size Dimensione in byte del segmento da creare.
    * @return int L'ID della memoria condivisa (shm_id).
    */
    int create_shared_memory(size_t size);

    /**
    * @brief Collega (attach) il segmento di memoria condivisa allo spazio di indirizzi del processo.
    * * @param shm_id L'ID del segmento di memoria condivisa.
    * @return void* Puntatore generico all'inizio della memoria condivisa.
    */
    void *attach_shared_memory(int shm_id);

    /**
    * @brief Crea un set di semafori.
    * * Utilizza `semget` per creare un array di semafori.
    * @param num_sems Numero di semafori da creare nel set.
    * @return int L'ID del set di semafori (sem_id).
    */
    int create_semaphore_set(int num_sems);

    /**
    * @brief Esegue l'operazione P (Wait/Lock) su un semaforo.
    * * Decrementa il valore del semaforo. Se il valore scende sotto 0, il processo si blocca.
    * @param sem_id L'ID del set di semafori.
    * @param sem_num L'indice del semaforo su cui operare (0-based).
    */
    void sem_wait(int sem_id, int sem_num);

    /**
    * @brief Esegue l'operazione V (Signal/Unlock) su un semaforo.
    * * Incrementa il valore del semaforo, svegliando eventuali processi in attesa.
    * @param sem_id L'ID del set di semafori.
    * @param sem_num L'indice del semaforo su cui operare (0-based).
    */
    void sem_signal(int sem_id, int sem_num);

    /**
    * @brief Imposta il valore iniziale di uno specifico semaforo.
    * * Utilizza `semctl` con comando SETVAL.
    * @param sem_id L'ID del set di semafori.
    * @param sem_num L'indice del semaforo da configurare.
    * @param value Il valore intero da assegnare (es. 1 per mutex, N per contatore).
    */
    void set_sem_value(int sem_id, int sem_num, int value);

    /**
    * @brief Crea una coda di messaggi.
    * * Utilizza `msgget` per creare la coda.
    * @return int L'ID della coda di messaggi (msq_id).
    */
    int create_message_queue();

    /**
    * @brief Rimuove tutte le risorse IPC dal sistema.
    * * Esegue la pulizia chiamando `shmctl`, `semctl` e `msgctl` con il comando IPC_RMID.
    * Deve essere chiamata dal processo Responsabile alla chiusura.
    * @param shm_id ID della memoria condivisa.
    * @param sem_id ID del set di semafori.
    * @param msq_id ID della coda di messaggi.
    */
    void cleanup_ipc_resources(int shm_id, int sem_id, int msq_id);

    /**
    * @brief Helper per i processi Client (Utente/Operatore) per connettersi alle IPC esistenti.
    * * Questa funzione ottiene le chiavi, recupera gli ID delle risorse create dal Responsabile
    * (senza usare IPC_CREAT) e esegue l'attach della memoria condivisa.
    * * @param sem_id Puntatore intero dove salvare l'ID dei semafori recuperato.
    * @param msq_id Puntatore intero dove salvare l'ID della coda messaggi recuperato.
    * @return GlobalState* Puntatore alla memoria condivisa collegata e castata a GlobalState.
    */
    GlobalState* connect_to_ipc(int *sem_id, int *msq_id);

#endif // IPC_UTILS_H
#include "./common.h"
#include "./ipc_utils.h"

/**
 * @brief Genera una chiave IPC unica e consistente utilizzando ftok.
 *
 * Questa funzione è essenziale per garantire che tutti i processi (Responsabile,
 * Operatori, Utenti) che tentano di accedere a una risorsa IPC (Memoria Condivisa,
 * Semafori, Code di Messaggi) ottengano la stessa chiave identificativa.
 *
 * @param path Il percorso di un file esistente (l'anchor file, es. "./ipc_key_anchor").
 * Deve essere lo stesso percorso per tutti i processi che cercano la risorsa.
 * @param proj_id L'identificatore del progetto (un intero distinto, es. 65, 99, 101)
 * che differenzia le varie risorse IPC che utilizzano lo stesso file anchor.
 * @return key_t La chiave IPC generata.
 * @retval -1 In caso di errore di ftok (tipicamente se il file specificato in 'path' non esiste).
 *
 * @note È responsabilità del processo Responsabile garantire che il file specificato in 'path'
 * esista prima di chiamare questa funzione.
 */
key_t get_ipc_key(const char *path, int proj_id){
    key_t key = ftok(path, proj_id);
    if(key == -1){
        perror("ftok error");
        exit(EXIT_FAILURE);
    }
    return key;
}

/** 
* @brief Crea un nuovo segmento di Memoria Condivisa (SHM). 
*
* Usa shmget con la flag IPC_CREAT. 
* 
* @param size La dimensione in byte della memoria da allocare. 
* @return int L'ID del segmento di Memoria Condivisa. 
* @retval -1 In caso di errore (es. memoria insufficiente). 
*/
int create_shared_memory(size_t size){
    if(size == 0) return -1;
    key_t key = get_ipc_key(IPC_KEY_FILE_PATH, SHM_ID_ASSIGNMENT_KEY);
    int shm_id = shmget(key, size, IPC_CREAT | IPC_EXCL | 0666);
    if(shm_id == -1){
        perror("shmget (shm) error");
        exit(EXIT_FAILURE);
    }
    return shm_id;
}

/**
 * @brief Collega (attach) un segmento di Memoria Condivisa allo spazio di indirizzamento del processo.
 *
 * Questa funzione rende accessibile il segmento di memoria identificato da 'shm_id'.
 * Dopo questa chiamata, il processo può leggere e scrivere nella memoria condivisa
 * come se fosse una normale variabile (tramite il puntatore restituito).
 *
 * @param shm_id L'identificatore del segmento di memoria (restituito da create_shared_memory o shmget).
 * * @return void* Un puntatore generico all'inizio del segmento di memoria condivisa.
 * Il chiamante dovrà fare il cast di questo puntatore alla struttura dati appropriata (es. GlobalState*).
 * * @retval (void*)-1 In caso di errore critico nella system call shmat (il programma termina con exit).
 */
void *attach_shared_memory(int shm_id){
    if(shm_id < 0) return NULL;

    void *att_shm =  shmat(shm_id, NULL, 0);    // NULL lascia che sia il sistema a scegliere l'indirizzo, 0 è il flag di default e permette lettura/scrittura
    if(att_shm == (void*)-1){
        perror("shmat error");
        exit(EXIT_FAILURE);
    }
    return att_shm;
}

/**
 * @brief Crea un nuovo Insieme di Semafori (Semaphore Set).
 *
 * Utilizza la chiave generata da get_ipc_key (con PROJ_ID specifico per semafori)
 * per allocare un array di semafori gestito dal kernel.
 *
 * @param num_sems Il numero totale di semafori da allocare nel set.
 * @return int L'ID dell'Insieme di Semafori (sem_id).
 * @retval -1 In caso di errore (il programma termina con exit).
 */
int create_semaphore_set(int num_sems){
    if(num_sems <= 0) return -1;
    key_t key = get_ipc_key(IPC_KEY_FILE_PATH, SEM_ID_ASSIGNMENT_KEY);
    int shm_id = semget(key, num_sems, IPC_CREAT | IPC_EXCL | 0666);
    if(shm_id == -1){
        perror("semget error");
        exit(EXIT_FAILURE);
    }
    return shm_id;
}

/**
 * @brief Esegue l'operazione P (Wait) su un semaforo specifico.
 *
 * Questa funzione tenta di decrementare il valore del semaforo di 1.
 * - Se il valore del semaforo è > 0: Il decremento avviene subito e il processo continua.
 * - Se il valore del semaforo è 0: Il processo viene BLOCCATO (messo in pausa dal Kernel)
 * fino a quando il semaforo non diventa disponibile (incrementato da un altro processo).
 *
 * Dettagli implementativi:
 * Utilizza la struct sembuf con:
 * - sem_num: l'indice passato come argomento.
 * - sem_op: -1 (Decremento/Wait).
 * - sem_flg: 0. (Bloccante, lo addormenta)
 *
 * @param sem_id L'ID dell'Insieme di Semafori (ottenuto da semget).
 * @param sem_num L'indice del semaforo all'interno del set (es. SEM_STAT_MUTEX).
 */
void sem_wait(int sem_id, int sem_num){
    // modulo da compliare con le infomazioni se si vuole agire su un semaforo
    struct sembuf sb;
    // compiliamo il modulo 
    sb.sem_num = sem_num;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    // consegnamo il modulo al Kernel
    if(semop(sem_id, &sb, 1) == -1){
        perror("sem_wait error");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Esegue l'operazione V (Signal) su un semaforo specifico.
 *
 * Questa funzione incrementa il valore del semaforo di 1.
 * Se c'erano processi bloccati in attesa su questo semaforo (tramite sem_wait),
 * uno di essi viene risvegliato dal Kernel e può procedere.
 *
 * Dettagli implementativi:
 * Utilizza la struct sembuf con:
 * - sem_num: l'indice passato come argomento.
 * - sem_op: 1 (Incremento/Signal).
 * - sem_flg: 0.
 *
 * @param sem_id L'ID dell'Insieme di Semafori.
 * @param sem_num L'indice del semaforo all'interno del set.
 */
void sem_signal(int sem_id, int sem_num){
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = 1;
    sb.sem_flg = 0;

    if(semop(sem_id, &sb, 1) == -1){    // 1 è il numero di operazioni
        perror("sem_signal error");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Rimuove le risorse IPC dal sistema (Cleanup).
 *
 * Questa funzione viene chiamata al termine dell'esecuzione (o in caso di errore fatale)
 * per marcare le risorse IPC per la distruzione.
 * Utilizza il flag IPC_RMID (Remove ID) con le system call shmctl, semctl e msgctl.
 *
 * È fondamentale chiamare questa funzione per evitare che rimangano risorse "zombie"
 * nel Kernel dopo la terminazione del programma.
 *
 * @param shm_id ID della Memoria Condivisa (passare -1 se non creata).
 * @param sem_id ID dell'Insieme di Semafori (passare -1 se non creato).
 * @param msq_id ID della Coda di Messaggi (passare -1 se non creata).
 */
void cleanup_ipc_resources(int shm_id, int sem_id, int msq_id){
    if(shm_id > 0){
        if(shmctl(shm_id, IPC_RMID, NULL) == -1){   // NULL, parametri puntatore ad una struct con info su della memoria
            perror("shmctl clenup error");
        }
    }

    if(sem_id > 0){
        if(semctl(sem_id, 0, IPC_RMID) == -1){  // 0, convenzione passare il primo indice anche se con il flag IPC_RMID elimina comunque tutto il set
            perror("semctl clenup error");
        }
    }

    if(msq_id > 0){
        if(msgctl(msq_id, IPC_RMID, NULL) == -1){
            perror("msqctl clenup error");
        }
    }
}

/**
 * @brief Crea una nuova Coda di Messaggi (Message Queue) per la comunicazione IPC.
 *
 * Utilizza la chiave generata con il Project ID specifico per le code di messaggi
 * (MSQ_ID_ASSIGNMENT_KEY) per allocare la coda nel kernel.
 * Questa coda sarà usata principalmente dal Responsabile per assegnare le stazioni agli Operatori.
 *
 * @return int L'ID della Coda di Messaggi (msq_id).
 * @retval -1 In caso di errore critico nella system call msgget (il programma termina con exit).
 */
int create_message_queue(){
    key_t key = get_ipc_key(IPC_KEY_FILE_PATH, MSQ_ID_ASSIGNMENT_KEY);
    int msq_id = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if(msq_id == -1){
        perror("msgget error");
        exit(EXIT_FAILURE);
    }
    return msq_id;
}

/**
 * @brief Imposta il valore iniziale di un semaforo specifico nel set.
 *
 * Questa funzione funge da wrapper per la system call `semctl` con comando SETVAL.
 * Viene utilizzata durante la fase di inizializzazione per configurare i semafori:
 * - A 1 per i semafori binari (Mutex).
 * - A N per i semafori contatori (es. numero di posti disponibili).
 *
 * Se l'impostazione fallisce, il programma termina con errore.
 *
 * @param sem_id  L'ID dell'insieme di semafori (ottenuto con semget).
 * @param sem_num L'indice del semaforo da configurare (es. SEM_MUTEX_STATS).
 * @param value   Il valore intero da assegnare al semaforo.
 */
void set_sem_value(int sem_id, int sem_num, int value) {
    if (semctl(sem_id, sem_num, SETVAL, value) == -1) {
        perror("semctl SETVAL error");
        exit(EXIT_FAILURE);
    }
}

GlobalState* connect_to_ipc(int *sem_id, int *msq_id){
    // A. Recupero delle IPC KEY
    key_t shm_key = get_ipc_key(IPC_KEY_FILE_PATH, SHM_ID_ASSIGNMENT_KEY);
    key_t sem_key = get_ipc_key(IPC_KEY_FILE_PATH, SEM_ID_ASSIGNMENT_KEY);
    key_t msq_key = get_ipc_key(IPC_KEY_FILE_PATH, MSQ_ID_ASSIGNMENT_KEY);
    if(shm_key == -1 || sem_key == -1 || msq_key == -1){
        perror("IPC key error");
        exit(EXIT_FAILURE);
    }

    // B. Ottenere gli ID
    int shm_id = shmget(shm_key, sizeof(GlobalState), 0666);
    if(shm_id == -1){
        perror("shmget (connect_to_ipc) error");
        exit(EXIT_FAILURE);
    }
    *sem_id = semget(sem_key, TOTAL_SEMAPHORES, 0666);
    if(*sem_id == -1){
        perror("semget error");
        exit(EXIT_FAILURE);
    }
    *msq_id = msgget(msq_key, 0666);
    if(*msq_id == -1){
        perror("msqget error");
        exit(EXIT_FAILURE);
    }

    // C. Attach Memoria Condivisa
    GlobalState *state = (GlobalState *) attach_shared_memory(shm_id);
    // printf("Utente %d: Accesso effettuato. Posti iniziali: %d\n", getpid(), state->config.NOF_TABLE_SEATS);

    return state;
}
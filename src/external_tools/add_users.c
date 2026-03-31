#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define USER_BIN "./bin/utente"

int main(int argc, char *argv[]){

    // 1. CONTROLLI INPUT
    if(argc < 2){
        printf("Uso: '%s <NUM_USERS>'\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_users = atoi(argv[1]);
    if(num_users < 1){
        printf("Nessun nuovo utente da creare (%d)\n", num_users);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_users; i++) {
        if (fork() == 0) {
            // Tutti i nuovi utenti sono singoli (ID 0)
            execl(USER_BIN, "utente", "0", "extra", (char *)NULL);
            perror("exec"); exit(1);
        }
    }
    printf("[NEW_USERS] %d utenti extra lanciati e registrati.\n", num_users);
    printf("[NEW_USERS] Il padre rimane in attesa della loro terminazione...\n");

    // --- LOGICA DI ATTESA ---
    // Questo ciclo blocca il padre finché TUTTI i figli non muoiono.
    // Impedisce alla shell di riprendere il controllo "sporcando" l'output.
    while (wait(NULL) > 0); 

    printf("[NEW_USERS] Tutti gli utenti extra hanno terminato. Chiudo.\n");
    return 0;
}
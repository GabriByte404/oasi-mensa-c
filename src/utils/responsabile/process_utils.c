#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "process_utils.h"

pid_t spawn_process(const char *program_name, char *arg1){
	pid_t pid = fork();
	if(pid < 0){
		perror("fork error");
		exit(EXIT_FAILURE);
	} else if(pid == 0){   // figlio
	// --- PROCESSO FIGLIO ---
		if (arg1 == NULL) {
			// Utile pre-inserimento logica Gruppi
			execl(program_name, program_name, (char *)NULL);
		} else {
			execl(program_name, program_name, arg1, (char *)NULL);
		}

		// Se arriviamo qui, execl ha fallito
		perror("Errore nella exec");
		exit(EXIT_FAILURE);
	}
	// Padre: ritorna il PID del figlio appena creato
	return pid;
}
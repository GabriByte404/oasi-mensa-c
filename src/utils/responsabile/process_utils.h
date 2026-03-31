/**
 * @file process_utils.h
 * @brief Funzioni di utilità per la creazione e gestione dei processi.
 * * Questo modulo fornisce un'interfaccia semplificata per la gestione 
 * delle system call di creazione processi (fork/exec), gestendo 
 * automaticamente il passaggio degli argomenti.
 */

#ifndef PROCESS_UTILS_H
    #define PROCESS_UTILS_H

    #include <sys/types.h> // Necessario per il tipo pid_t

    /**
    * @brief Genera un nuovo processo figlio ed esegue il programma specificato.
    * * La funzione esegue una `fork()`. Nel processo figlio, viene chiamata la famiglia
    * di funzioni `exec` (solitamente `execl`) per sostituire l'immagine del processo
    * corrente con quella dell'eseguibile indicato.
    * * @param program_name Il percorso del file eseguibile (es. "./bin/operatore").
    * @param arg1 Primo argomento opzionale da passare all'eseguibile (può essere NULL).
    * Se la `fork` fallisce, la funzione gestisce l'errore (solitamente terminando).
    */
    pid_t spawn_process(const char *program_name, char *arg1);

#endif // PROCESS_UTILS_H
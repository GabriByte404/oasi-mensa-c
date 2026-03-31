/**
 * @file config.h
 * @brief Modulo per il parsing e il caricamento della configurazione.
 * * Questo file definisce l'interfaccia per leggere i parametri di simulazione
 * da un file di testo esterno (es. config.conf) e popolare la struttura dati
 * condivisa utilizzata da tutti i processi.
 */

#ifndef CONFIG_H
    #define CONFIG_H

    #include "common.h" // Necessario per la definizione di ConfigData

    /**
    * @brief Legge il file di configurazione e popola la struttura ConfigData.
    * * La funzione apre il file specificato, legge le righe ignorando i commenti (righe che iniziano con '#')
    * e le righe vuote. Cerca coppie nel formato "CHIAVE=VALORE" (es. "SIM_DURATION=30")
    * e assegna i valori ai campi corrispondenti della struttura `config`.
    * * @param filename Il percorso relativo o assoluto del file di configurazione (es. "./config/config.conf").
    * @param config Puntatore a una struttura `ConfigData` già allocata che verrà riempita.
    * * @return int Restituisce 0 in caso di successo, -1 se il file non può essere aperto o se si verifica un errore di parsing.
    */
    int load_config(const char *filename, ConfigData *config);

#endif // CONFIG_H
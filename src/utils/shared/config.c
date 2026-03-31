#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

int load_config(const char *filename, ConfigData *config) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Errore apertura file config");
        return -1;
    }

    char line[256];
    char key[128];
    int value_int;

    // Imposta valori di default (sicurezza)
    config->SIM_DURATION = 30; 
    config->MAX_PORZIONI_PRIMI = 50;
    config->MAX_PORZIONI_SECONDI = 50;
    
    while (fgets(line, sizeof(line), file)) {
        // Salta commenti (#) e righe vuote
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        // Cerca pattern "CHIAVE=VALORE"
        // Nota: Questo assume che NON ci siano spazi attorno all'uguale nel file .conf
        if (sscanf(line, "%[^=]=%d", key, &value_int) == 2) {
            
            // --- 1. Generali e Popolazione ---
            if (strcmp(key, "SIM_DURATION") == 0) config->SIM_DURATION = value_int;
            else if (strcmp(key, "NOF_USERS") == 0) config->NOF_USERS = value_int;
            else if (strcmp(key, "NOF_WORKERS") == 0) config->NOF_WORKERS = value_int;
            else if (strcmp(key, "OVERLOAD_THRESHOLD") == 0) config->OVERLOAD_THRESHOLD = value_int;
            
            // --- 2. Posti ---
            else if (strcmp(key, "NOF_TABLE_SEATS") == 0) config->NOF_TABLE_SEATS = value_int;
            else if (strcmp(key, "NOF_WK_SEATS_PRIMI") == 0) config->NOF_WK_SEATS_PRIMI = value_int;
            else if (strcmp(key, "NOF_WK_SEATS_SECONDI") == 0) config->NOF_WK_SEATS_SECONDI = value_int;
            else if (strcmp(key, "NOF_WK_SEATS_COFFEE") == 0) config->NOF_WK_SEATS_COFFEE = value_int;
            else if (strcmp(key, "NOF_WK_SEATS_CASSA") == 0) config->NOF_WK_SEATS_CASSA = value_int;

            // --- 3. Tempi di Servizio (AVG_SRVC) ---
            else if (strcmp(key, "AVG_SRVC_PRIMI") == 0) config->AVG_SRVC_PRIMI = value_int;
            else if (strcmp(key, "AVG_SRVC_SECONDI") == 0) config->AVG_SRVC_SECONDI = value_int;
            else if (strcmp(key, "AVG_SRVC_COFFEE") == 0) config->AVG_SRVC_COFFEE = value_int;
            else if (strcmp(key, "AVG_SRVC_CASSA") == 0) config->AVG_SRVC_CASSA = value_int;

            // --- 4. Prezzi (PRICE) ---
            else if (strcmp(key, "PRICE_PRIMI") == 0) config->PRICE_PRIMI = (double)value_int;
            else if (strcmp(key, "PRICE_SECONDI") == 0) config->PRICE_SECONDI = (double)value_int;
            else if (strcmp(key, "PRICE_COFFEE") == 0) config->PRICE_COFFEE = (double)value_int;

            // --- 5. Refill (Standard) ---
            else if (strcmp(key, "REFILL_THRESHOLD") == 0) config->REFILL_THRESHOLD = value_int;
            else if (strcmp(key, "REFILL_AMOUNT") == 0) config->REFILL_AMOUNT = value_int;
            else if (strcmp(key, "AVG_REFILL_TIME") == 0) config->AVG_REFILL_TIME = value_int;
            else if (strcmp(key, "NOF_PAUSES") == 0) config->NOF_PAUSES = value_int;

            // --- 6. Refill & Capacità (NUOVI - ERANO MANCANTI) ---
            else if (strcmp(key, "AVG_REFILL_PRIMI") == 0) config->AVG_REFILL_PRIMI = value_int;
            else if (strcmp(key, "AVG_REFILL_SECONDI") == 0) config->AVG_REFILL_SECONDI = value_int;
            else if (strcmp(key, "MAX_PORZIONI_PRIMI") == 0) config->MAX_PORZIONI_PRIMI = value_int;
            else if (strcmp(key, "MAX_PORZIONI_SECONDI") == 0) config->MAX_PORZIONI_SECONDI = value_int;

            // --- 7 Gruppi
            else if (strcmp(key, "MAX_USERS_PER_GROUP") == 0) config->MAX_USERS_PER_GROUP = value_int;
            
            // --- 7. Speciale (Long) ---
            else if (strcmp(key, "N_NANO_SECS") == 0) {
                // Rilegge la riga per prendere il long
                sscanf(line, "%*[^=]=%ld", &config->N_NANO_SECS);
            }
        }
    }

    fclose(file);
    return 0;
}
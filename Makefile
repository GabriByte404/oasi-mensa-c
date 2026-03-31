# Target principale: se scrivi 'make' esegue tutto
all: clean crea_cartella responsabile utente operatore disorder add_users

# 1. Crea la cartella 'bin' se non esiste
crea_cartella:
	@mkdir -p bin

# 2. Compila il RESPONSABILE
# Prende responsabile.c E tutti i file .c che sono dentro utils
responsabile:
	gcc -Wvla -Wextra -Werror -D_GNU_SOURCE -o bin/responsabile src/responsabile.c src/utils/shared/ipc_utils.c src/utils/shared/config.c src/utils/responsabile/report_utils.c src/utils/responsabile/process_utils.c src/utils/responsabile/simulation_utils.c

# 3. Compila l'UTENTE
utente:
	gcc -Wvla -Wextra -Werror -D_GNU_SOURCE -o bin/utente src/utente.c src/utils/shared/ipc_utils.c src/utils/utente/utente_utils.c

# 4. Compila l'OPERATORE
operatore:
	gcc -Wvla -Wextra -Werror -D_GNU_SOURCE -o bin/operatore src/operatore.c src/utils/shared/ipc_utils.c src/utils/operatore/operatore_utils.c

# 4. Compila il DISORDER
disorder:
	gcc -Wvla -Wextra -Werror -D_GNU_SOURCE -o bin/disorder src/external_tools/disorder.c src/utils/shared/ipc_utils.c

# 4. Compila l'ADD_USERS
add_users:
	gcc -Wvla -Wextra -Werror -D_GNU_SOURCE -o bin/add_users src/external_tools/add_users.c

# Pulizia: cancella i file generati per ripartire da zero
clean:
	@rm -f bin/responsabile bin/utente bin/operatore bin/add_users bin/disorder
	@rm -f ipc_key_anchor
	@clear
	@echo "Pulizia Completata..."
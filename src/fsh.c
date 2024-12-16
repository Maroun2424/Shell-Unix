#include "../include/command_executor.h"
#include "../include/prompt.h"
#include "../include/for.h"
#include "../include/commandes_internes.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char *input;
    char current_dir[1024];

    while (1) {
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("Error retrieving the current directory");
            continue;
        }

        // Affiche le prompt
        rl_outstream = stderr;
        char *prompt = update_prompt(last_exit_status, current_dir);
        input = readline(prompt);

    if (input == NULL) { // Gestion Ctrl-D
        write(STDOUT_FILENO, "\n", 1);
        cmd_exit(NULL); // Utilise last_exit_status pour quitter
        break; // Facultatif car cmd_exit appelle exit()
    }

        if (strcmp(input, "") == 0) { // Commande vide
            free(input);
            continue;
        }

        add_history(input);
        process_command(input);

        free(input);
    }

    return 0;
}

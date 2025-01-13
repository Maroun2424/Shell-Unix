#include "../include/fsh.h"
#include "../include/for.h"
#include "../include/commandes_internes.h"
#include "../include/commandes_simples.h"
#include "../include/prompt.h"
#include "../include/redirections.h"
#include "../include/command_executor.h"
#include "../include/prompt.h"
#include "../include/for.h"
#include "../include/commandes_internes.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <string.h>

volatile sig_atomic_t sigint_received = 0;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        last_exit_status = -sig;
        sigint_received = 1;
    }
}

int main() {
    char *input;
    char current_dir[1024];

    // Configurer la gestion des signaux
    struct sigaction sa_default = {0}, sa_ignore = {0};
    sa_default.sa_handler = handle_signal;
    sa_ignore.sa_handler = SIG_IGN;

    while (1) {
        // Ignorer SIGINT et SIGTERM dans le shell principal
        sigaction(SIGINT, &sa_ignore, NULL);
        sigaction(SIGTERM, &sa_ignore, NULL);

        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("Error retrieving the current directory");
            continue;
        }

        // Affiche le prompt
        rl_outstream = stderr;
        char *prompt = update_prompt(last_exit_status, current_dir);
        input = readline(prompt);

        // Restaurer la gestion de SIGINT et SIGTERM après readline()
        sigaction(SIGINT, &sa_default, NULL);
        sigaction(SIGTERM, &sa_default, NULL);

        if (input == NULL) { // Gestion Ctrl-D
            write(STDOUT_FILENO, "\n", 1);
            cmd_exit(NULL); // Utilise last_exit_status pour quitter
            break; // Facultatif car cmd_exit appelle exit()
        }

        if (strcmp(input, "") == 0) {
            free(input);
            continue;
        }

        add_history(input);
        process_command(input);

        if (sigint_received) {
            last_exit_status = -SIGINT;
            sigint_received = 0;
        }

        free(input);
    }

    return 0;
}
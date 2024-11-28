#include "../include/commandes_internes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdbool.h>
#include <signal.h>

#define COLOR_GREEN "\001\033[32m\002"
#define COLOR_RED "\001\033[91m\002"
#define COLOR_BLUE "\001\033[34m\002"
#define COLOR_RESET "\001\033[0m\002"
#define MAX_PROMPT_LENGTH 512

int last_exit_status = 0; // Statut de la dernière commande exécutée

/**
 * @brief Quitte le shell.
 *
 * @param exit_code_str Code de retour ou NULL pour utiliser last_exit_status.
 */
void cmd_exit(const char *exit_code_str) {
    int exit_code = (exit_code_str) ? atoi(exit_code_str) : last_exit_status;
    exit(exit_code);
}

/**
 * @brief Met à jour le prompt du shell.
 *
 * @param last_exit_status Dernier statut de commande.
 * @param current_dir Répertoire courant.
 * @return Chaîne représentant le prompt.
 */
char* update_prompt(int last_exit_status, const char *current_dir) {
    static char prompt[512];
    const char *status_color = (last_exit_status == 0) ? COLOR_GREEN : COLOR_RED;
    char truncated_dir[256] = "";

    // Tronque le répertoire si nécessaire
    int len = strlen(current_dir);
    if (len > 29) {
        strcpy(truncated_dir, "...");
        strcat(truncated_dir, current_dir + (len - 22));
    } else {
        strcpy(truncated_dir, current_dir);
    }

    // Construit le prompt
    strcpy(prompt, status_color);
    strcat(prompt, "[");
    if (last_exit_status == 0) {
        strcat(prompt, "0");
    } else {
        char val[3];
        snprintf(val, 3, "%d", last_exit_status);
        strcat(prompt, val);
    }
    strcat(prompt, "]");
    strcat(prompt, COLOR_BLUE);
    strcat(prompt, truncated_dir);
    strcat(prompt, COLOR_RESET);
    strcat(prompt, "$ ");

    return prompt;
}

/**
 * @brief Exécute une commande.
 *
 * @param input Commande entrée par l'utilisateur.
 */
void process_command(char *input) {
    char *args[100];
    int arg_count = 0;
    bool first_token = true;

    // Sépare la commande en arguments
    char *command;
    while ((command = strtok(first_token ? input : NULL, " ")) != NULL && arg_count < 100) {
        args[arg_count++] = command;
        first_token = false;
    }
    args[arg_count] = NULL;

    if (args[0] == NULL) return;

    // Commandes internes
    if (strcmp(args[0], "cd") == 0) {
        if (arg_count > 2) {
            fprintf(stderr, "Error: Too many arguments for 'cd'\n");
            last_exit_status = 1;
        } else {
            const char *path = arg_count > 1 ? args[1] : NULL;
            last_exit_status = cmd_cd(path);
        }
    } else if (strcmp(args[0], "pwd") == 0) {
        if (arg_count > 1) {
            fprintf(stderr, "Error: Too many arguments for 'pwd'\n");
            last_exit_status = 1;
        } else {
            last_exit_status = cmd_pwd();
        }
    } else if (strcmp(args[0], "exit") == 0) {
        if (arg_count > 2) {
            fprintf(stderr, "Error: Too many arguments for 'exit'\n");
            last_exit_status = 1;
        } else {
            int exit_val = args[1] ? atoi(args[1]) : last_exit_status;
            exit(exit_val);
        }
    } else if (strcmp(args[0], "for") == 0) {
        last_exit_status = simple_for_loop(args);
    } else if (strcmp(args[0], "ftype") == 0) {
        if (arg_count != 2) {
            fprintf(stderr, "Error: 'ftype' command requires exactly one argument.\n");
            last_exit_status = 1;
        } else {
            last_exit_status = cmd_ftype(args[1]);
        }
    } else {
        // Commandes externes
        pid_t pid = fork();

        if (pid == 0) { // Processus enfant
            if (strlen(args[0]) > 2 && args[0][0] == '.' && args[0][1] == '/') {
                execv(args[0], args);
            } else {
                execvp(args[0], args);
            }
            perror("execvp");
            exit(EXIT_FAILURE);

        } else if (pid > 0) { // Processus parent
            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status)) {
                last_exit_status = WEXITSTATUS(status);
            } else {
                last_exit_status = 1;
            }
        } else { // Échec du fork
            perror("fork");
            last_exit_status = 1;
        }
    }
}

/**
 * @brief Point d'entrée du programme shell.
 */
int main() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    char *input;
    char current_dir[1024];

    // Boucle principale du shell
    while (1) {
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("Error retrieving the current directory");
            continue;
        }

        // Gère le prompt
        rl_outstream = stderr;
        char *prompt = update_prompt(last_exit_status, current_dir);
        input = readline(prompt);

        if (input == NULL) { // Gestion Ctrl-D
            write(STDOUT_FILENO, "\n", 1);
            cmd_exit(NULL);
            break;
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

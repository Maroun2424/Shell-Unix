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

int last_exit_status = 0;  // Ce qui va nous permettre d'afficher [0] si l'opération précédente a réussi ou [1] si elle a échoué

/**
 * @brief Quitte le shell avec une valeur de retour spécifiée ou la dernière valeur de retour.
 *
 * @param exit_code Chaîne contenant la valeur de retour ou NULL pour utiliser last_exit_status.
 */
void cmd_exit(const char *exit_code_str) {
    int exit_code = (exit_code_str) ? atoi(exit_code_str) : last_exit_status;
    exit(exit_code);
}

char* update_prompt(int last_exit_status, const char *current_dir) {
    static char prompt[512]; // Buffer statique pour le prompt
    const char *status_color = (last_exit_status == 0) ? COLOR_GREEN : COLOR_RED;
    char truncated_dir[256] = "";

    // Tronquer le répertoire si nécessaire
    int len = strlen(current_dir);
    if (len > 29) {
        strcpy(truncated_dir, "...");
        strcat(truncated_dir, current_dir + (len - 22));  // Ajoute la partie tronquée du répertoire
    } else {
        strcpy(truncated_dir, current_dir);
    }

    // Construire le prompt
    strcpy(prompt, status_color);     // Début avec la couleur de statut
    strcat(prompt, "[");
    if (last_exit_status == 0) {
        strcat(prompt, "0");          // Statut de succès
    } else {
        strcat(prompt, "1");          // Statut d'erreur
    }
    strcat(prompt, "]");
    strcat(prompt, COLOR_BLUE);       // Couleur pour le répertoire
    strcat(prompt, truncated_dir);    // Ajout du répertoire
    strcat(prompt, COLOR_RESET);      // Reset la couleur
    strcat(prompt, "$ ");             // Fin du prompt

    return prompt; // Renvoie le prompt construit
}


void process_command(char *input) {
    char *args[100];
    int arg_count = 0;
    bool first_token = true;

    // Split the command into arguments
    char *command;
    while ((command = strtok(first_token ? input : NULL, " ")) != NULL && arg_count < 100) {
        args[arg_count++] = command;
        first_token = false;
    }
    args[arg_count] = NULL; // Terminate the arguments list with NULL

    if (args[0] == NULL) {
        return; // No command to execute
    }

    /** Internal Commands **/
    if (strcmp(args[0], "cd") == 0) {
        if (arg_count > 2) {
            fprintf(stderr, "Error: Too many arguments for 'cd'\n");
            last_exit_status = 1;
        } else {
            const char *path = arg_count > 1 ? args[1] : NULL; // If no argument, pass NULL
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
            fprintf(stderr, "Erreur : La commande 'ftype' nécessite exactement un argument.\n");
            last_exit_status = 1;
        } else {
            last_exit_status = cmd_ftype(args[1]);
        }
    } else {
        /** External Commands **/
        pid_t pid = fork();

        if (pid == 0) { // Child process
            execvp(args[0], args); // Try to execute the command
            perror("execvp");
            exit(EXIT_FAILURE); // Exit if failure
        } else if (pid > 0) { // Parent process
            int status;
            waitpid(pid, &status, 0); // Wait for the child to finish
            if (WIFEXITED(status)) {
                last_exit_status = WEXITSTATUS(status); // Retrieve the return code
            } else {
                last_exit_status = 1;
            }
        } else { // Fork failed
            perror("fork");
            last_exit_status = 1;
        }
    }
}



void signal_handler() {
    // À utiliser si nécessaire
}

int main() {

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    char *input;  // Stocke l'entrée de l'utilisateur
    char current_dir[1024];

    // Boucle principale du shell
    while (1) {
    // Récupérer le répertoire courant
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perror("Erreur lors de la récupération du répertoire courant");
        continue;  // Passer à la prochaine itération en cas d'erreur
    }

    // Générer l'invite de commande
    rl_outstream = stderr;
    char *prompt = update_prompt(last_exit_status, current_dir);
    input = readline(prompt);

    // Gérer Ctrl-D (input == NULL)
    if (input == NULL) {
        write(STDOUT_FILENO, "\n", 1);  // Affiche une nouvelle ligne propre
        cmd_exit(NULL);  // Quitte le shell
        break;
    }

    // Si la commande est vide, continuer la boucle sans la traiter
    if (strcmp(input, "") == 0) {
        free(input);
        continue;
    }

    if(strcmp(input, "") != 0) {
        add_history(input);
        process_command(input);
    }

    // Libérer la mémoire allouée pour l'entrée utilisateur
    free(input);
}


    return 0;
}

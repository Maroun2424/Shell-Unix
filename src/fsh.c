#include "../include/commandes_internes.h"
#include "../include/commandes_externes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>

#define COLOR_GREEN "\001\033[32m\002"
#define COLOR_RED "\001\033[91m\002"
#define COLOR_BLUE "\001\033[34m\002"
#define COLOR_RESET "\001\033[0m\002"

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
    if (len > 30) {
        strcpy(truncated_dir, "...");
        strcat(truncated_dir, current_dir + (len - 27));  // Ajoute la partie tronquée du répertoire
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



void signal_handler(int signum) {
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
        
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("Erreur lors de la récupération du répertoire courant");
        }

        char *prompt = update_prompt(last_exit_status, current_dir);
        input = readline(prompt);
    
        if (input == NULL) {
            write(STDOUT_FILENO, "\n", 1); // Pour gérer Ctrl-D proprement
            cmd_exit(NULL); // Exécuter exit sans paramètre sur Ctrl-D
            break;
        }

        add_history(input);  // Ajoute la commande à l'historique

        if (strncmp(input, "exit", 4) == 0) {
            char *exit_arg = input + 5; // Pointer juste après "exit "
            while (*exit_arg == ' ') exit_arg++; // Ignorer les espaces
            cmd_exit(*exit_arg ? exit_arg : NULL);  // Exécute exit avec ou sans paramètre
        } else if (strncmp(input, "cd", 2) == 0) {
            const char *path = strtok(input + 2, " ");
            if (path != NULL && strcmp(path, "") != 0)
                last_exit_status = cmd_cd(path);
            else
                last_exit_status = cmd_cd(NULL); 
        } else if (strcmp(input, "pwd") == 0) {
            last_exit_status = cmd_pwd();  // Appel de la commande cmd_pwd
        } else if (strncmp(input, "ftype ", 6) == 0) {
            last_exit_status = cmd_ftype(input + 6);
        } else if (strncmp(input, "simplefor", 9) == 0) {
            char *args = strtok(input + 10, " "); // +10 pour passer "simplefor "
            char *directory = args;
            char *command = strtok(NULL, "");

            if (directory && command) {
                simple_for_loop(directory, command);
                last_exit_status = 0;  // Supposer que tout s'est bien passé
            } else {
                fprintf(stderr, "Usage: simplefor <directory> <command>\n");
                last_exit_status = 1;
            }
        } else {
            last_exit_status = execute_external_command(input);
        }

        free(input);  // Libère la mémoire allouée pour l'entrée utilisateur
    }
    
    return 0;
}

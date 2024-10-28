#include "../include/commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>       // Pour pid_t
#include <sys/wait.h>        // Pour waitpid
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>


#define COLOR_GREEN "\001\033[32m\002"
#define COLOR_RED "\001\033[91m\002"
#define COLOR_BLUE "\001\033[34m\002"
#define COLOR_CYAN "\001\033[36m\002"
#define COLOR_RESET "\001\033[00m\002"

int last_exit_status = 0;  // Ce qui va nous permettre d'afficher [0] si l'opération précédente a réussi ou [1] si elle a échoué



void update_prompt(char *prompt, int last_exit_status, const char *current_dir) {
    const char *status_color = (last_exit_status == 0) ? COLOR_GREEN : COLOR_RED;
    char truncated_dir[256];

    if (strlen(current_dir) > 30) {
        snprintf(truncated_dir, sizeof(truncated_dir), "...%s", current_dir + (strlen(current_dir) - 27));
    } else {
        strncpy(truncated_dir, current_dir, sizeof(truncated_dir));
    }

    snprintf(prompt, 256, "%s[%d] \033[34m%s\033[00m$ ", status_color, last_exit_status, truncated_dir);
}

void signal_handler(int signum) {
    
}


int main() {
    char *input;  // Stocke l'entrée de l'utilisateur
    char prompt[256];  
    char current_dir[1024];

    // Boucle principale du shell
    while (1) {
        
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("Erreur lors de la récupération du répertoire courant");
        }
        update_prompt(prompt, last_exit_status, current_dir);
        input = readline(prompt);

        if (input == NULL) {
            break;  // Sortie sur EOF (Ctrl-D)
        }

        add_history(input);  // Ajoute la commande à l'historique

        /*if (strncmp(input, "exit", 4) == 0) {
            free(input);
            cmd_exit(0);  // Appel de la fonction cmd_exit pour quitter proprement
        } else*/

        if (strncmp(input, "cd ", 3) == 0) {
            last_exit_status = cmd_cd(input + 3);  // Appel de la commande cd
        } else if (strcmp(input, "pwd") == 0) {
            last_exit_status = cmd_pwd();  // Appel de la commande cmd_pwd
        } else if (strncmp(input, "ftype ", 6) == 0) {
            last_exit_status = cmd_ftype(input + 6);  // Appel de la commande ftype avec le chemin fourni
        } else {
            pid_t pid = fork();
            if (pid == 0) {
                // Processus enfant : exécute la commande
                execlp("/bin/sh", "sh", "-c", input, (char *)NULL);
                write(STDERR_FILENO, "Erreur: commande introuvable\n", 29);
                _exit(127);  // Si execlp échoue
            } else if (pid > 0) {
                // Processus parent : attend la fin du processus enfant
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                    last_exit_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    last_exit_status = 128 + WTERMSIG(status);
                }
            } else {
                write(STDERR_FILENO, "Erreur: échec du fork\n", 22);
            }
        }

        free(input);  // Libère la mémoire allouée pour l'entrée utilisateur
    }

    return 0;
}  

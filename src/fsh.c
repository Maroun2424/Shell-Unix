#include "../include/commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>       // Pour pid_t
#include <sys/wait.h>        // Pour waitpid
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char *input;  // Stocke l'entrée de l'utilisateur
    char prompt[256];  

    // Boucle principale du shell
    while (1) {
        snprintf(prompt, sizeof(prompt), "fsh> ");  // Définit l'invite de commande
        input = readline(prompt);  // Récupère l'entrée de l'utilisateur

        if (input == NULL) {
            break;  // Sortie sur EOF (Ctrl-D)
        }

        add_history(input);  // Ajoute la commande à l'historique

        /*if (strncmp(input, "exit", 4) == 0) {
            free(input);
            cmd_exit(0);  // Appel de la fonction cmd_exit pour quitter proprement
        } else*/ if (strncmp(input, "cd ", 3) == 0) {
            cmd_cd(input + 3);  // Appel de la commande cd
        } else if (strcmp(input, "pwd") == 0) {
            cmd_pwd();  // Appel de la commande cmd_pwd
        } else if (strncmp(input, "ftype ", 6) == 0) {
            cmd_ftype(input + 6);  // Appel de la commande ftype avec le chemin fourni
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
            } else {
                write(STDERR_FILENO, "Erreur: échec du fork\n", 22);
            }
        }

        free(input);  // Libère la mémoire allouée pour l'entrée utilisateur
    }

    return 0;
}  

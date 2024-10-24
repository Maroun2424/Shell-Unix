#include "../include/commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char *input;  // Pour stocker l'entrée de l'utilisateur
    char prompt[256];  // Prompt du shell

    // Boucle principale du shell
    while (1) {
        snprintf(prompt, sizeof(prompt), "fsh> ");  // Créez votre invite de commande
        input = readline(prompt);  // Lire une ligne de commande

        if (input == NULL) {
            break;  // Sortie sur EOF (Ctrl-D)
        }

        add_history(input);  // Ajoutez la commande à l'historique

        // Traitez la commande
        if (strncmp(input, "exit", 4) == 0) {
            free(input);
            break;
        } else if (strncmp(input, "cd ", 3) == 0) {
            cmd_cd(input + 3);  // Appel de la commande cd
        } else if (strcmp(input, "pwd") == 0) {
            cmd_pwd();  // Appel de la commande pwd
        } else {
            system(input);  // Exécutez les commandes externes
        }

        free(input);  // Libérez la mémoire
    }

    return 0;
}

#include "../include/commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char *input;  // Stock l'entrée de l'utilisateur
    char prompt[256];  

    // Boucle principale du shell
    while (1) {
        snprintf(prompt, sizeof(prompt), "fsh> ");  // 
        input = readline(prompt);  // 

        if (input == NULL) {
            break;  // Sortie sur EOF (Ctrl-D)
        }

        add_history(input);  // Ajout de la commande à l'historique

        if (strncmp(input, "exit", 4) == 0) {
            free(input);
            break;
        } else if (strncmp(input, "cd ", 3) == 0) {
            cmd_cd(input + 3);  // Appel de la commande cd
        } else if (strcmp(input, "pwd") == 0) {
            cmd_pwd();  // Appel de la commande pwd
        } else {
            system(input); 
        }

        free(input);  
    }

    return 0;
}

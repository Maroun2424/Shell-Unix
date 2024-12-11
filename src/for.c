#include "../include/commandes_internes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <ctype.h>
#include <linux/limits.h> 
#include <linux/limits.h> 
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include "../include/commandes_simples.h"
#include "../include/fsh.h" // Pour `last_exit_status`
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>

/**
 * @brief Parcourt les fichiers d'un répertoire et exécute une commande sur chaque fichier.
 *
 * @param args Tableau de chaînes représentant les arguments.
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */
int simple_for_loop(char *args[]) {
    if (!args || !args[1] || !args[2] || !args[3] || !args[5]) {
        fprintf(stderr, "Invalid arguments\n");
        return -1;
    }

    const char *directory = args[3];
    DIR *dir;
    struct dirent *entry;

    dir = opendir(directory);
    if (dir == NULL) {
        perror("Error opening directory");
        return -1;
    }

    // Vérifie la syntaxe "for F in REP { CMD }"
    if (strcmp(args[0], "for") != 0 || strcmp(args[2], "in") != 0 || strcmp(args[4], "{") != 0) {
        fprintf(stderr, "Syntax error: Command must follow 'for F in REP { CMD }'\n");
        closedir(dir);
        return -1;
    }

    // Prépare la variable $F
    char varname[256];
    snprintf(varname, sizeof(varname), "$%s", args[1]);

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue; // Ignore les fichiers cachés
        }

        char command_buffer[1024] = ""; // Buffer pour stocker la commande finale

        // Parcourt les arguments pour construire la commande
        for (int i = 5; args[i] && strcmp(args[i], "}") != 0; ++i) {
            const char *arg = args[i];
            char temp[1024] = ""; // Buffer temporaire pour traiter l'argument
            int temp_index = 0;  // Index pour temp

            while (*arg) {
                if (*arg == '$' && strncmp(arg, varname, strlen(varname)) == 0) {
                    // Si on trouve $F, remplacer par le chemin complet
                    snprintf(temp + temp_index, sizeof(temp) - temp_index, "%s/%s",
                             directory, entry->d_name);
                    temp_index += strlen(temp + temp_index); // Avance dans le buffer
                    arg += strlen(varname); // Passe après $F
                } else {
                    // Copie le caractère courant
                    temp[temp_index++] = *arg;
                    temp[temp_index] = '\0';
                    arg++;
                }
            }

            strcat(command_buffer, temp); // Ajoute l'argument au buffer final
            strcat(command_buffer, " "); // Ajoute un espace
        }

        // Exécute la commande générée
        command_buffer[strlen(command_buffer) - 1] = '\0'; // Supprime l'espace final
        process_command(command_buffer);
    }

    closedir(dir);
    return 0;
}

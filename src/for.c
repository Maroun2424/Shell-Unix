#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>
#include "../include/fsh.h"
#include "../include/commandes_internes.h"
#include "../include/commandes_simples.h"
#include "../include/command_executor.h"

// Structure pour les informations de boucle
typedef struct {
    char *var_name;
    char *directory;
    char **command_parts;
    int command_parts_count;
    int show_hidden;
    int recursive;
    char *extension_filter;
    char *type_filter;
} for_loop_t;

// Analyse et prépare les données de la commande `for`
int parse_for_command(char *args[], for_loop_t *loop) {
    memset(loop, 0, sizeof(for_loop_t));

    if (!args[0] || strcmp(args[0], "for") != 0 || !args[1] || !args[2] || strcmp(args[2], "in") != 0 || !args[3]) {
        fprintf(stderr, "Syntax error: Invalid 'for' command\n");
        return -1;
    }

    loop->var_name = args[1];
    loop->directory = args[3];

    int i = 4;
    while (args[i]) {
        // Gère les options jusqu'à trouver '{'
        if (strcmp(args[i], "-A") == 0) {
            loop->show_hidden = 1;
        } else if (strcmp(args[i], "-r") == 0) {
            loop->recursive = 1;
        } else if (strcmp(args[i], "-e") == 0) {
            if (!args[i + 1]) {
                fprintf(stderr, "Error: Missing value for -e\n");
                return -1;
            }
            loop->extension_filter = args[++i];
        } else if (strcmp(args[i], "-t") == 0) {
            if (!args[i + 1]) {
                fprintf(stderr, "Error: Missing value for -t\n");
                return -1;
            }
            loop->type_filter = args[++i];
        } else if (strcmp(args[i], "{") == 0) {
            // Commence à lire la commande après '{'
            i++;
            loop->command_parts = &args[i];

            // Compte le nombre d'arguments jusqu'à '}'
            while (args[i] && strcmp(args[i], "}") != 0) {
                loop->command_parts_count++;
                i++;
            }

            // Vérifie qu'on a trouvé '}'
            if (!args[i] || strcmp(args[i], "}") != 0) {
                fprintf(stderr, "Syntax error: Missing '}'\n");
                return -1;
            }
            break;
        } else {
            fprintf(stderr, "Syntax error: Unexpected argument '%s'\n", args[i]);
            return -1;
        }
        i++;
    }

    if (!loop->command_parts || loop->command_parts_count == 0) {
        fprintf(stderr, "Syntax error: Missing command to execute\n");
        return -1;
    }

    return 0;
}


// Exécute la commande construite
void execute_for_command(const char *cmd_buffer) {
    if (cmd_buffer && strlen(cmd_buffer) > 0) {
        process_command((char *)cmd_buffer);
    }
}

int handle_for_iteration(for_loop_t *loop) {
    DIR *dir = opendir(loop->directory);
    if (!dir) {
        perror("Error opening directory");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue; // Ignore . et ..
        if (entry->d_name[0] == '.' && !loop->show_hidden) continue;

        // Filtrage par extension
        if (loop->extension_filter) {
            char *dot = strrchr(entry->d_name, '.');
            if (!dot || strcmp(dot + 1, loop->extension_filter) != 0) continue;
        }

        // Construction du chemin complet
        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", loop->directory, entry->d_name);

        // Construction de la commande avec substitutions
        char cmd_buffer[2048] = "";
        for (int i = 0; i < loop->command_parts_count; i++) {
            const char *arg = loop->command_parts[i];
            if (strcmp(arg, "$F") == 0) {
                strncat(cmd_buffer, file_path, sizeof(cmd_buffer) - strlen(cmd_buffer) - 1);
            } else if (strcmp(arg, "$D") == 0) {
                strncat(cmd_buffer, loop->directory, sizeof(cmd_buffer) - strlen(cmd_buffer) - 1);
            } else {
                strncat(cmd_buffer, arg, sizeof(cmd_buffer) - strlen(cmd_buffer) - 1);
            }
            strncat(cmd_buffer, " ", sizeof(cmd_buffer) - strlen(cmd_buffer) - 1);
        }

        cmd_buffer[strlen(cmd_buffer) - 1] = '\0'; // Retirer l'espace final
        execute_for_command(cmd_buffer);
    }

    closedir(dir);
    return 0;
}


int simple_for_loop(char *args[]) {
    for_loop_t loop;
    if (parse_for_command(args, &loop) == -1) {
        return -1;
    }

    return handle_for_iteration(&loop);
}

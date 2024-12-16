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
    char command_buffer[1024] = "";
    int brace_count = 0;

    while (args[i]) {
        if (strcmp(args[i], "{") == 0) {
            brace_count++;
        } else if (strcmp(args[i], "}") == 0) {
            brace_count--;
            if (brace_count == 0) break; // Fin du bloc
        } else if (brace_count > 0) {
            // Collecte tout ce qu'il y a dans le bloc
            strncat(command_buffer, args[i], sizeof(command_buffer) - strlen(command_buffer) - 1);
            strncat(command_buffer, " ", sizeof(command_buffer) - strlen(command_buffer) - 1);
        }
        i++;
    }

    if (brace_count != 0) {
        fprintf(stderr, "Syntax error: Unmatched '{'\n");
        return -1;
    }

    // Enregistre la commande collectée
    loop->command_parts = malloc(sizeof(char *));
    loop->command_parts[0] = strdup(command_buffer);
    loop->command_parts_count = 1;

    return 0;
}


// Gère l'itération de la boucle `for`
int handle_for_iteration(for_loop_t *loop) {
    struct stat st;

    // Vérifie si le chemin existe et est un répertoire
    if (stat(loop->directory, &st) == -1) {
        perror("Error opening directory");
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a directory\n", loop->directory);
        return -1;
    }


    DIR *dir = opendir(loop->directory);
    if (!dir) {
        perror("Error opening directory");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore . et ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (entry->d_name[0] == '.' && !loop->show_hidden) continue;

        // Filtrage par extension
        if (loop->extension_filter) {
            char *dot = strrchr(entry->d_name, '.');
            if (!dot || strcmp(dot + 1, loop->extension_filter) != 0) continue;
        }

        // Chemin complet
        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", loop->directory, entry->d_name);

        // Substitution de $F
        char cmd_buffer[2048] = "";
        const char *cmd_template = loop->command_parts[0];
        const char *start = cmd_template;


        while (*start) {
            const char *pos = strstr(start, "$F");
            if (pos) {
                strncat(cmd_buffer, start, pos - start);
                strncat(cmd_buffer, file_path, sizeof(cmd_buffer) - strlen(cmd_buffer) - 1);
                start = pos + 2;
            } else {
                strncat(cmd_buffer, start, sizeof(cmd_buffer) - strlen(cmd_buffer) - 1);
                break;
            }
        }
        if (strstr(cmd_buffer, "for ") != NULL) {
        } else {
            process_command(cmd_buffer);
        }   
    }

    closedir(dir);
    return 0;
}

// Exécute une boucle `for`
int simple_for_loop(char *args[]) {

    for_loop_t loop;
    if (parse_for_command(args, &loop) == -1) {
        return -1;
    }

    int result = handle_for_iteration(&loop);

    return result;
}

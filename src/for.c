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
    char type_filter; // Nouveau : filtre par type (f, d, l, p)
} for_loop_t;

// Fonction pour vérifier la syntaxe et initialiser la structure `for_loop_t`
int initialize_loop(char *args[], for_loop_t *loop) {
    memset(loop, 0, sizeof(for_loop_t));

    if (!args[0] || strcmp(args[0], "for") != 0 || !args[1] || !args[2] || strcmp(args[2], "in") != 0 || !args[3]) {
        fprintf(stderr, "Syntax error: Invalid 'for' command\n");
        return -1;
    }

    loop->var_name = args[1];
    loop->directory = args[3];
    loop->show_hidden = 0;
    loop->extension_filter = NULL;
    loop->type_filter = '\0'; // Aucun filtre par défaut
    loop->recursive = 0; // Pas récursif par défaut

    // Parcours des arguments après REP pour détecter les options
    for (int i = 4; args[i] && strcmp(args[i], "{") != 0; i++) {
        if (strcmp(args[i], "-A") == 0) {
            loop->show_hidden = 1; // Affiche les fichiers cachés
        } else if (strcmp(args[i], "-e") == 0) {
            if (args[i + 1]) {
                loop->extension_filter = strdup(args[i + 1]);
                i++; // Passe à l'extension suivante
            } else {
                fprintf(stderr, "Syntax error: Missing extension after '-e'\n");
                return -1;
            }
        } else if (strcmp(args[i], "-t") == 0) {
            if (args[i + 1] && strchr("fdlp", args[i + 1][0])) { // Vérifie si l'argument est valide
                loop->type_filter = args[i + 1][0];
                i++;
            } else {
                fprintf(stderr, "Syntax error: Invalid or missing type after '-t'\n");
                return -1;
            }
        } else if (strcmp(args[i], "-r") == 0) {
            loop->recursive = 1; // Active la récursivité
        } else {
            fprintf(stderr, "Syntax error: Unknown option '%s'\n", args[i]);
            return -1;
        }
    }
    return 0;
}





// Fonction pour analyser les commandes à exécuter dans le bloc `{ }`
int parse_command_block(char *args[], int start_index, for_loop_t *loop) {
    char command_buffer[1024] = "";
    int brace_count = 0;

    for (int i = start_index; args[i]; i++) {
        if (strcmp(args[i], "{") == 0) {
            brace_count++;
        } else if (strcmp(args[i], "}") == 0) {
            brace_count--;
            if (brace_count == 0) break; // Fin du bloc
        } else if (brace_count > 0) {
            strncat(command_buffer, args[i], sizeof(command_buffer) - strlen(command_buffer) - 1);
            strncat(command_buffer, " ", sizeof(command_buffer) - strlen(command_buffer) - 1);
        }
    }

    if (brace_count != 0) {
        fprintf(stderr, "Syntax error: Unmatched '{'\n");
        return -1;
    }

    loop->command_parts = malloc(sizeof(char *));
    loop->command_parts[0] = strdup(command_buffer);
    loop->command_parts_count = 1;

    return 0;
}

// Fonction pour appliquer le filtrage par extension ou par fichier caché
int should_include_file(const struct dirent *entry, const for_loop_t *loop, const char *dir_path) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) return 0;
    if (entry->d_name[0] == '.' && !loop->show_hidden) return 0;

    // Filtrage par extension
    if (loop->extension_filter) {
        char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot + 1, loop->extension_filter) != 0) return 0;
    }

    // Construction du chemin complet
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

    // Filtrage par type si l'option -t est activée
    if (loop->type_filter) {
        struct stat st;
        if (lstat(file_path, &st) == -1) return 0; // Erreur dans l'accès aux stats

        switch (loop->type_filter) {
            case 'f': if (!S_ISREG(st.st_mode)) return 0; break;
            case 'd': if (!S_ISDIR(st.st_mode)) return 0; break;
            case 'l': if (!S_ISLNK(st.st_mode)) return 0; break;
            case 'p': if (!S_ISFIFO(st.st_mode)) return 0; break;
            default:
                fprintf(stderr, "Error: Unknown type filter '%c'\n", loop->type_filter);
                return 0;
        }
    }

    return 1;
}


// Fonction pour générer la commande finale avec la substitution de `$F`
void generate_command_with_substitution(const char *template, const char *file_path, const for_loop_t *loop, char *cmd_buffer, size_t buffer_size) {
    const char *start = template;
    cmd_buffer[0] = '\0';

    while (*start) {
        const char *pos = strstr(start, "$F");
        if (pos) {
            strncat(cmd_buffer, start, pos - start);

            if (loop->extension_filter) {
                // Retirer l'extension du fichier
                char base_name[PATH_MAX];
                strncpy(base_name, file_path, sizeof(base_name));
                char *dot = strrchr(base_name, '.');
                if (dot) *dot = '\0'; // Couper l'extension
                strncat(cmd_buffer, base_name, buffer_size - strlen(cmd_buffer) - 1);
            } else {
                strncat(cmd_buffer, file_path, buffer_size - strlen(cmd_buffer) - 1);
            }

            start = pos + 2; // Saute le "$F"
        } else {
            strncat(cmd_buffer, start, buffer_size - strlen(cmd_buffer) - 1);
            break;
        }
    }
}


// Fonction pour traiter chaque fichier dans le répertoire
void process_directory(const for_loop_t *loop) {
    struct stat st;

    // Vérifie si le chemin existe et est un répertoire
    if (stat(loop->directory, &st) == -1) {
        perror("Error opening directory");
        return;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a directory\n", loop->directory);
        return;
    }

    DIR *dir = opendir(loop->directory);
    if (!dir) {
        perror("Error opening directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!should_include_file(entry, loop, loop->directory)) continue;

        // Chemin complet
        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", loop->directory, entry->d_name);

        // Vérifie si c'est un sous-répertoire pour la récursivité
        struct stat entry_stat;
        if (stat(file_path, &entry_stat) == 0 && S_ISDIR(entry_stat.st_mode) && loop->recursive) {
            // Appelle récursivement la fonction sur le sous-répertoire
            for_loop_t sub_loop = *loop;
            sub_loop.directory = file_path;
            process_directory(&sub_loop);
        }

        // Substitution de `$F`
        char cmd_buffer[2048];
        generate_command_with_substitution(loop->command_parts[0], file_path, loop, cmd_buffer, sizeof(cmd_buffer));

        // Exécute la commande générée
        process_command(cmd_buffer);
    }

    closedir(dir);
}



// Fonction principale pour exécuter une boucle `for`
int simple_for_loop(char *args[]) {
    for_loop_t loop;

    // Initialisation et parsing de la commande `for`
    if (initialize_loop(args, &loop) == -1) return -1;

    // Analyse du bloc de commandes
    if (parse_command_block(args, 4, &loop) == -1) {
        free(loop.command_parts[0]);
        free(loop.command_parts);
        return -1;
    }

    // Traitement des fichiers dans le répertoire
    process_directory(&loop);

    // Libération des ressources
    free(loop.command_parts[0]);
    free(loop.command_parts);

    return 0;
}

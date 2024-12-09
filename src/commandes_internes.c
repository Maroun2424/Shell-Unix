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

#define MAX_PATH_LENGTH 1024

#ifndef O_DIRECTORY
#define O_DIRECTORY 0200000
#endif

/**
 * @brief Change le répertoire courant.
 *
 * @param path Chemin vers le répertoire cible.
 * @return 0 en cas de succès, 1 en cas d'erreur.
 */
int cmd_cd(const char *path) {
    static char previous_path[MAX_PATH_LENGTH];
    char current_path[MAX_PATH_LENGTH];

    if (getcwd(current_path, sizeof(current_path)) == NULL) {
        perror("cd: Impossible de récupérer le répertoire courant");
        return 1;
    }

    // Gestion de "cd" sans argument ou avec "-"
    if (path == NULL || strcmp(path, "") == 0) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "cd: HOME non défini\n");
            return 1;
        }
    } else if (strcmp(path, "-") == 0) {
        if (strlen(previous_path) == 0) {
            fprintf(stderr, "cd: OLDPWD non défini\n");
            return 1;
        }
        path = previous_path;
    }

    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }

    strncpy(previous_path, current_path, sizeof(previous_path) - 1);
    previous_path[sizeof(previous_path) - 1] = '\0';

    return 0;
}

/**
 * @brief Affiche le répertoire courant.
 *
 * @return 0 en cas de succès, 1 en cas d'erreur.
 */
int cmd_pwd() {
    char path[MAX_PATH_LENGTH];

    if (getcwd(path, sizeof(path)) == NULL) {
        perror("pwd: Impossible de récupérer le répertoire courant");
        return 1;
    }

    write(STDOUT_FILENO, path, strlen(path));
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}

/**
 * @brief Supprime les espaces autour d'une chaîne.
 *
 * @param str Chaîne à modifier.
 */
void trim_whitespace(char *str) {
    char *end;

    while (isspace((unsigned char)*str)) str++; // Supprime les espaces au début
    if (*str == 0) return; // Chaîne vide

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--; // Supprime les espaces à la fin

    *(end + 1) = '\0'; // Termine la chaîne
}

/**
 * @brief Vérifie et affiche le type d'un fichier ou répertoire.
 *
 * @param path Chemin du fichier ou répertoire.
 * @return 0 en cas de succès, 1 en cas d'erreur.
 */
int cmd_ftype(const char *path) {
    struct stat file_stat;
    char trimmed_path[MAX_PATH_LENGTH];

    strncpy(trimmed_path, path, sizeof(trimmed_path));
    trim_whitespace(trimmed_path);

    if (lstat(trimmed_path, &file_stat) == -1) {
        perror("ftype");
        return 1;
    }

    if (S_ISDIR(file_stat.st_mode)) {
        write(STDOUT_FILENO, "directory\n", 10);
    } else if (S_ISREG(file_stat.st_mode)) {
        write(STDOUT_FILENO, "regular file\n", 13);
    } else if (S_ISLNK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "symbolic link\n", 14);
    } else if (S_ISFIFO(file_stat.st_mode)) {
        write(STDOUT_FILENO, "named pipe\n", 11);
    } else if (S_ISSOCK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "socket\n", 7);
    } else {
        write(STDOUT_FILENO, "other\n", 6);
    }

    return 0;
}

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

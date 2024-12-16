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
#include "../include/command_executor.h" // Pour `last_exit_status`
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>


#define MAX_PATH_LENGTH 1024

#ifndef O_DIRECTORY
#define O_DIRECTORY 0200000
#endif


/**
 * @brief Quitte le shell.
 *
 * @param exit_code_str Code de retour ou NULL pour utiliser last_exit_status.
 */
void cmd_exit(const char *exit_code_str) {
    int exit_code = (exit_code_str) ? atoi(exit_code_str) : last_exit_status;
    exit_code &= 0xFF; // Contraint la valeur à l'octet inférieur (0-255)
    exit(exit_code);
}

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

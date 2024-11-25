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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/limits.h>

#define MAX_PATH_LENGTH 1024

// Assure que O_DIRECTORY est défini pour open
#ifndef O_DIRECTORY
#define O_DIRECTORY 0200000
#endif

/**
 * @brief Change le répertoire de travail.
 *
 * Modifie le répertoire de travail courant du processus vers celui spécifié par le chemin. 
 * Si le chemin est invalide ou si une erreur survient, un message d'erreur est affiché. 
 * Si "cd" est utilisé seul, le répertoire change pour HOME. Si "cd" est lancé avec "-", 
 * il retourne au dernier répertoire visité.
 *
 * @param path Pointeur vers la chaîne de caractères contenant le chemin
 *             du répertoire à changer.
 */


int cmd_cd(const char *path) {

    static char previous_path[MAX_PATH_LENGTH];
    char current_path[MAX_PATH_LENGTH];

    if (getcwd(current_path, sizeof(current_path)) == NULL) {
        perror("cd: Error retrieving the current directory");
        return 1;
    }

    if (path == NULL || strcmp(path, "") == 0) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(path, "-") == 0) {
        if (strlen(previous_path) == 0) {
            fprintf(stderr, "cd: OLDPWD not set\n");
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
 * @brief Obtient et affiche le chemin du répertoire courant.
 *
 * Utilise getcwd pour récupérer le chemin du répertoire courant
 * et l'affiche sur la sortie standard.
 *
 * @return 0 en cas de succès, 1 en cas d'erreur.
 */
int cmd_pwd() {
    char path[MAX_PATH_LENGTH];

    if (getcwd(path, sizeof(path)) == NULL) {
        perror("Erreur lors de la récupération du chemin actuel");
        return 1;
    }

    write(STDOUT_FILENO, path, strlen(path));
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}

/**
 * @brief Supprime les espaces en début et en fin d'une chaîne.
 *
 * Modifie la chaîne d'entrée pour enlever les espaces vides avant le premier 
 * et après le dernier caractère non blanc. 
 *
 * @param str Pointeur vers la chaîne à traiter (modifiée en place).
 */
void trim_whitespace(char *str) {
    char *end;

    while (isspace((unsigned char)*str)) str++;  // Trim leading spaces

    if (*str == 0)  // Check if the string is empty
        return;

    end = str + strlen(str) - 1;  // Set end pointer to the last character
    while (end > str && isspace((unsigned char)*end)) end--;  // Trim trailing spaces

    *(end + 1) = 0;  // Null terminate after last non-space character
}

/**
 * @brief Vérifie et affiche le type d'un fichier ou répertoire.
 *
 * Cette fonction utilise lstat pour récupérer les informations
 * de statut d'un fichier ou répertoire spécifié par le chemin. 
 * Elle affiche le type correspondant.
 *
 * @param path Pointeur vers la chaîne de caractères contenant le chemin à vérifier.
 */
int cmd_ftype(const char *path) {
    struct stat file_stat;
    char trimmed_path[MAX_PATH_LENGTH];

    // Copie du chemin et suppression des espaces
    strncpy(trimmed_path, path, sizeof(trimmed_path));
    trim_whitespace(trimmed_path);

    // Tente d'obtenir les informations de statut pour le fichier ou répertoire
    if (lstat(trimmed_path, &file_stat) == -1) {
        perror("ftype");
        return 1;
    }

    // Vérifie et affiche le type de fichier
    if (S_ISDIR(file_stat.st_mode)) {
        write(STDOUT_FILENO, "directory", 9);
        write(STDOUT_FILENO, "\n", 1);
    } else if (S_ISREG(file_stat.st_mode)) {
        write(STDOUT_FILENO, "regular file", 12);
        write(STDOUT_FILENO, "\n", 1);
    } else if (S_ISLNK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "symbolic link", 13);
        write(STDOUT_FILENO, "\n", 1);
    } else if (S_ISFIFO(file_stat.st_mode)) {
        write(STDOUT_FILENO, "named pipe", 10);
        write(STDOUT_FILENO, "\n", 1);
    } else if (S_ISSOCK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "socket", 6);
        write(STDOUT_FILENO, "\n", 1);
    } else {
        write(STDOUT_FILENO, "other", 5);
        write(STDOUT_FILENO, "\n", 1);
    }
    return 0;
}

/**
 * Execute une commande simple pour chaque fichier dans un répertoire donné.
 *
 * @param directory Le chemin du répertoire à parcourir.
 * @param command La commande à exécuter pour chaque fichier.
 */
int simple_for_loop(const char *command,const char *directory) {
    DIR *dir;
    struct dirent *entry;
    char command_buffer[1024];

    dir = opendir(directory);
    if (dir == NULL) {
        perror("Error opening directory");
        return -1; // Return an error status
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue; // Skip hidden files
        }

        // Build the full command to execute for each file
        int command_length = snprintf(command_buffer, sizeof(command_buffer), "%s %s/%s", command, directory, entry->d_name);
        if (command_length >= sizeof(command_buffer)) {
            fprintf(stderr, "Command buffer overflow\n");
            return -1; // Set error status due to buffer overflow
            break; // Exit the loop to avoid executing an incomplete command
        }

        // Execute the command
        if (system(command_buffer) == -1) {
            perror("Error executing command");
            return -1; // Set error status due to command execution failure
        }
    }

    closedir(dir);
    return 0; // Return the final status of the operation
}

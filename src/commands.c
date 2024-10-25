#include "../include/commands.h"
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

#define MAX_PATH_LENGTH 1024  

// Assure que O_DIRECTORY est défini pour open
#ifndef O_DIRECTORY
#define O_DIRECTORY 0200000
#endif

/**
 * @brief Change le répertoire de travail.
 *
 * Modifie le répertoire de travail courant du processus
 * à celui spécifié par le chemin. Si le chemin est invalide ou si
 * une erreur survient, un message d'erreur est affiché.
 *
 * @param path Pointeur vers la chaîne de caractères contenant le chemin
 *             du répertoire à changer.
 */
void cmd_cd(const char *path) {
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        write(STDERR_FILENO, "Erreur: impossible d'ouvrir le répertoire\n", 42);
        return;
    }
    if (fchdir(fd) == -1) {
        write(STDERR_FILENO, "Erreur: impossible de changer de répertoire\n", 44);
    }
    close(fd);
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
        write(STDERR_FILENO, "Erreur: impossible d'obtenir le chemin actuel\n", 46);
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
void cmd_ftype(const char *path) {
    struct stat file_stat;
    char trimmed_path[PATH_MAX];

    // Copie du chemin et suppression des espaces
    strncpy(trimmed_path, path, sizeof(trimmed_path));
    trim_whitespace(trimmed_path);

    // Affiche le chemin complet pour le débogage
    fprintf(stderr, "Vérification du type pour : '%s'\n", trimmed_path);

    // Tente d'obtenir les informations de statut pour le fichier ou répertoire
    if (lstat(trimmed_path, &file_stat) == -1) {
        printf("Le fichier '%s' n'a pas pu être trouvé dans le système.\n", trimmed_path);
        perror("Erreur lors de l'appel à lstat");
        return;
    }

    // Vérifie et affiche le type de fichier
    if (S_ISDIR(file_stat.st_mode)) {
        write(STDOUT_FILENO, "répertoire\n", 11);
    } else if (S_ISREG(file_stat.st_mode)) {
        write(STDOUT_FILENO, "fichier ordinaire\n", 18);
    } else if (S_ISLNK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "lien symbolique\n", 16);
        
        // Affiche le chemin que le lien symbolique pointe
        char link_target[PATH_MAX];
        ssize_t len = readlink(trimmed_path, link_target, sizeof(link_target) - 1);
        if (len != -1) {
            link_target[len] = '\0'; // Null-terminate the target string
            printf("Pointe vers : %s\n", link_target);
        } else {
            perror("Erreur lors de la lecture du lien symbolique");
        }
    } else if (S_ISCHR(file_stat.st_mode)) {
        write(STDOUT_FILENO, "périphérique de caractère\n", 27);
    } else if (S_ISBLK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "périphérique de bloc\n", 21);
    } else if (S_ISFIFO(file_stat.st_mode)) {
        write(STDOUT_FILENO, "FIFO (tube nommé)\n", 19);
    } else if (S_ISSOCK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "socket\n", 7);
    } else {
        write(STDOUT_FILENO, "type inconnu\n", 13);
    }
}

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

static char last_dir[MAX_PATH_LENGTH] = ""; // Stockage du dernier répertoire

int cmd_cd(const char *path) {
    char current_dir[MAX_PATH_LENGTH];
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perror("Erreur lors de la récupération du répertoire courant");
        return 1;
    }

    // Si aucun argument n'est fourni, utiliser le répertoire HOME
    if (path == NULL || strcmp(path, "") == 0) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(path, "-") == 0) { // Gestion de 'cd -' pour revenir au dernier répertoire
        if (strlen(last_dir) == 0) {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
        path = last_dir; 
    }

    // Vérification si le chemin existe
    if (access(path, F_OK) != 0) {
        fprintf(stderr, "cd: no such file or directory: %s\n", path);
        return 1;
    }

    // Vérification si le chemin est un répertoire
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        perror("cd: erreur lors de la vérification du chemin");
        return 1;
    }
    if (!S_ISDIR(path_stat.st_mode)) {
        fprintf(stderr, "cd: not a directory: %s\n", path);
        return 1;
    }

    // Changement de répertoire
    if (chdir(path) != 0) {
        perror("cd: erreur lors du changement de répertoire");
        return 1;
    }

    // Mise à jour du dernier répertoire visité
    strncpy(last_dir, current_dir, sizeof(last_dir) - 1);
    last_dir[sizeof(last_dir) - 1] = '\0';

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
    char trimmed_path[PATH_MAX];

    // Copie du chemin et suppression des espaces
    strncpy(trimmed_path, path, sizeof(trimmed_path));
    trim_whitespace(trimmed_path);

    // Affiche le chemin complet pour le débogage
    fprintf(stderr, "Vérification du type pour : '%s'\n", trimmed_path);

    // Tente d'obtenir les informations de statut pour le fichier ou répertoire
    if (lstat(trimmed_path, &file_stat) == -1) {
        perror("Erreur lors de l'appel à lstat");
        return 1;
    }

    // Vérifie et affiche le type de fichier
    if (S_ISDIR(file_stat.st_mode)) {
        write(STDOUT_FILENO, "répertoire", 11);
        write(STDOUT_FILENO, "\n",1);
    } else if (S_ISREG(file_stat.st_mode)) {
        write(STDOUT_FILENO, "fichier ordinaire\n", 18);
        write(STDOUT_FILENO, "\n",1);
    } else if (S_ISLNK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "lien symbolique\n", 16);
        write(STDOUT_FILENO, "\n",1);
        
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
    return 0;
}

/**
 * Execute une commande simple pour chaque fichier dans un répertoire donné.
 *
 * @param directory Le chemin du répertoire à parcourir.
 * @param command La commande à exécuter pour chaque fichier.
 */
void simple_for_loop(const char *directory, const char *command) {
    DIR *dir;
    struct dirent *entry;
    char command_buffer[1024];

    if ((dir = opendir(directory)) == NULL) {
        perror("Erreur lors de l'ouverture du répertoire");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue; // Skip hidden files
        }

        // Construire la commande complète à exécuter pour le fichier
        snprintf(command_buffer, sizeof(command_buffer), "%s %s/%s", command, directory, entry->d_name);

        // Exécuter la commande
        if (system(command_buffer) == -1) {
            perror("Erreur lors de l'exécution de la commande");
        }
    }

    closedir(dir);
}

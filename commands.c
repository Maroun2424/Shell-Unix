#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#define MAX_PATH_LENGTH 1024  

// Fonction pour trouver le nom du fichier dans le répertoire parent correspondant à l'inode donné
char* get_filename_from_inode(ino_t inode, const char *parent) {
    DIR *dir = opendir(parent);
    if (!dir) {
        perror("opendir");
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_ino == inode) {
            closedir(dir);
            return strdup(entry->d_name); // Retourne une copie du nom du fichier
        }
    }
    closedir(dir);
    return NULL;
}

int pwd() {
    struct stat current_stat, parent_stat;
    char path[MAX_PATH_LENGTH] = "";  // Le chemin absolu sera stocké ici
    char *filename;
    
    // On commence dans le répertoire courant
    if (stat(".", &current_stat) == -1) {
        perror("stat");
        return 1;
    }

    while (1) {
        // On obtient l'inode du répertoire parent
        if (stat("..", &parent_stat) == -1) {
            perror("stat");
            return 1;
        }

        // Si on est à la racine (inode parent == inode actuel), on arrête
        if (current_stat.st_ino == parent_stat.st_ino) {
            break;
        }

        // On récupère le nom du répertoire correspondant à l'inode dans le répertoire parent
        filename = get_filename_from_inode(current_stat.st_ino, "..");
        if (!filename) {
            return 1;
        }

        // Vérifie que la concaténation ne dépasse pas la taille limite du buffer
        size_t path_len = strlen(path);
        size_t filename_len = strlen(filename);

        if (path_len + filename_len + 2 >= MAX_PATH_LENGTH) {
            fprintf(stderr, "Path length exceeds the limit\n");
            free(filename);
            return 1;
        }

        // On ajoute le nom du répertoire au début du chemin
        memmove(path + filename_len + 1, path, path_len + 1); // Décale l'ancien chemin
        memcpy(path, "/", 1);  // Ajoute le slash
        memcpy(path + 1, filename, filename_len);  // Copie le nom du répertoire
        free(filename);

        // On se déplace dans le répertoire parent
        if (chdir("..") == -1) {
            perror("chdir");
            return 1;
        }

        // On met à jour l'inode du répertoire courant
        if (stat(".", &current_stat) == -1) {
            perror("stat");
            return 1;
        }
    }

    // Si le chemin est vide, on est à la racine
    if (strlen(path) == 0) {
        strcpy(path, "/");
    }

    printf("%s\n", path);  // Affiche le chemin absolu
    return 0;
}

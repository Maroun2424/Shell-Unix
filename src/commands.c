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

#define MAX_PATH_LENGTH 1024  

// Assure que O_DIRECTORY est défini pour open
#ifndef O_DIRECTORY
#define O_DIRECTORY 0200000
#endif

// Fonction pour changer de répertoire
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

// Fonction pwd pour obtenir le chemin actuel
int cmd_pwd() {
    char path[MAX_PATH_LENGTH];

    // Utiliser getcwd pour obtenir le chemin actuel
    if (getcwd(path, sizeof(path)) == NULL) {
        write(STDERR_FILENO, "Erreur: impossible d'obtenir le chemin actuel\n", 46);
        return 1;
    }

    // Afficher le chemin
    write(STDOUT_FILENO, path, strlen(path));
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}



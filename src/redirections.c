#include "../include/redirections.h"
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <errno.h>

//Détecte le type de redirection
TypeDeRedirection getTypeRed(const char *token) {
    if (strcmp(token, "<") == 0) return REDIR_INPUT;
    if (strcmp(token, ">") == 0) return REDIR_OUTPUT;
    if (strcmp(token, ">|") == 0) return REDIR_FORCE_OUTPUT;
    if (strcmp(token, ">>") == 0) return REDIR_APPEND_OUTPUT;
    if (strcmp(token, "2>") == 0) return REDIR_ERROR;
    if (strcmp(token, "2>|") == 0) return REDIR_FORCE_ERROR;
    if (strcmp(token, "2>>") == 0) return REDIR_APPEND_ERROR;
    return REDIR_INCONNU;
}

//Applique la redirection indiquée
int appliqueRedirection(TypeDeRedirection type, const char *filename) {
    int fd = -1;
    int target_fd;

    switch (type) {
        case REDIR_INPUT:         target_fd = STDIN_FILENO;  fd = open(filename, O_RDONLY); break;
        case REDIR_OUTPUT:        target_fd = STDOUT_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0664); break;
        case REDIR_FORCE_OUTPUT:  target_fd = STDOUT_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664); break;
        case REDIR_APPEND_OUTPUT: target_fd = STDOUT_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0664); break;
        case REDIR_ERROR:         target_fd = STDERR_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0664); break;
        case REDIR_FORCE_ERROR:   target_fd = STDERR_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664); break;
        case REDIR_APPEND_ERROR:  target_fd = STDERR_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0664); break;
        default:                  fprintf(stderr, "Type de redirection inconnu\n"); return 1;
    }

    if (fd == -1) {
        if (errno == EEXIST) {
            if (target_fd == STDOUT_FILENO) {
                fprintf(stderr, "File exists\n");
            } else if (target_fd == STDERR_FILENO) {
                fprintf(stderr, "File exists\n");
            } else {
                fprintf(stderr, "Erreur lors de l'ouverture du fichier: ");
                perror("open");
            }
        } else {
            fprintf(stderr, "Erreur lors de l'ouverture du fichier: ");
            perror("open");
        }
        return 1;
    }

    if (dup2(fd, target_fd) == -1) {
        fprintf(stderr, "Erreur lors de la redirection du descripteur de fichier: ");
        perror("dup2");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

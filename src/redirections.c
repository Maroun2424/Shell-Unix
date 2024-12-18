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
        case REDIR_OUTPUT:        target_fd = STDOUT_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664); break;
        case REDIR_FORCE_OUTPUT:  target_fd = STDOUT_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664); break;
        case REDIR_APPEND_OUTPUT: target_fd = STDOUT_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0664); break;
        case REDIR_ERROR:         target_fd = STDERR_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664); break;
        case REDIR_FORCE_ERROR:   target_fd = STDERR_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664); break;
        case REDIR_APPEND_ERROR:  target_fd = STDERR_FILENO; fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0664); break;
        errno = EINVAL; perror("Redirection inconnue");

    }

    if (fd == -1) {
        perror("open");
        return 1;
    }

    if (dup2(fd, target_fd) == -1) {
        perror("dup2");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int manage_redirections(char **args, int *arg_count,
                        char **infile, char **outfile, char **errfile,
                        TypeDeRedirection *out_type, TypeDeRedirection *err_type) {
    *infile = NULL;
    *outfile = NULL;
    *errfile = NULL;
    *out_type = REDIR_INCONNU;
    *err_type = REDIR_INCONNU;

    for (int i = 0; i < *arg_count; i++) {
        TypeDeRedirection redir = getTypeRed(args[i]);
        if (redir != REDIR_INCONNU) {
            if (i + 1 >= *arg_count) {
                errno = EINVAL; perror("Redirection sans fichier");
                return 1;
            }

            char *filename = args[i + 1];

            switch (redir) {
                case REDIR_INPUT:
                    if (*infile != NULL) {
                        errno = EEXIST; perror("Redirection input multiple");
                        return 1;
                    }
                    *infile = filename;
                    break;

                case REDIR_OUTPUT:
                case REDIR_FORCE_OUTPUT:
                case REDIR_APPEND_OUTPUT:
                    if (*outfile != NULL) {
                        errno = EEXIST; perror("Redirection output multiple");
                        return 1;
                    }
                    *outfile = filename;
                    *out_type = redir;
                    break;

                case REDIR_ERROR:
                case REDIR_FORCE_ERROR:
                case REDIR_APPEND_ERROR:
                    if (*errfile != NULL) {
                        errno = EEXIST; perror("Redirection error multiple");
                        return 1;
                    }
                    *errfile = filename;
                    *err_type = redir;
                    break;

                default:
                    errno = EINVAL; perror("Redirection inconnue");
                    return 1;
            }

            // Supprime les deux tokens (la redirection et le fichier) du tableau d'arguments
            memmove(&args[i], &args[i + 2], (*arg_count - i - 2) * sizeof(char *));
            *arg_count -= 2;
            args[*arg_count] = NULL;
            i--; // On réévalue la même position au prochain tour
        }
    }

    return 0;
}
#include "../include/redirections.h"
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#define PATH_MAX 4096

// Détecte le type de redirection en fonction d'une chaîne isolée ("<", ">>", etc.)
TypeDeRedirection getTypeRed(const char *token) {
    if (strcmp(token, "<")   == 0) return REDIR_INPUT;
    if (strcmp(token, ">")   == 0) return REDIR_OUTPUT;
    if (strcmp(token, ">|")  == 0) return REDIR_FORCE_OUTPUT;
    if (strcmp(token, ">>")  == 0) return REDIR_APPEND_OUTPUT;
    if (strcmp(token, "2>")  == 0) return REDIR_ERROR;
    if (strcmp(token, "2>|") == 0) return REDIR_FORCE_ERROR;
    if (strcmp(token, "2>>") == 0) return REDIR_APPEND_ERROR;
    return REDIR_INCONNU;
}

// Applique la redirection indiquée
int appliqueRedirection(TypeDeRedirection type, const char *filename) {
    int fd = -1;
    int target_fd;

    switch (type) {
        case REDIR_INPUT:
            target_fd = STDIN_FILENO;
            fd = open(filename, O_RDONLY);
            break;

        case REDIR_OUTPUT:
            target_fd = STDOUT_FILENO;
            // O_EXCL => échoue si le fichier existe déjà
            fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0664);
            break;

        case REDIR_FORCE_OUTPUT:
            target_fd = STDOUT_FILENO;
            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);
            break;

        case REDIR_APPEND_OUTPUT:
            target_fd = STDOUT_FILENO;
            fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0664);
            break;

        case REDIR_ERROR:
            target_fd = STDERR_FILENO;
            fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0664);
            break;

        case REDIR_FORCE_ERROR:
            target_fd = STDERR_FILENO;
            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);
            break;

        case REDIR_APPEND_ERROR:
            target_fd = STDERR_FILENO;
            fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0664);
            break;

        case REDIR_INCONNU:
        default:
            perror("Redirection inconnue");
            return 1;
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


static int split_redirection_token(const char *token, char *redir_str, char *filename_str) {
    // On liste les opérateurs du plus long au plus court pour repérer
    // "2>>", "2>|", "2>", ">>", ">|", ">", "<"
    static const char *ops[] = {
        "2>>", "2>|", "2>", ">>", ">|", ">", "<"
    };
    const int nb_ops = sizeof(ops)/sizeof(ops[0]);

    for (int i = 0; i < nb_ops; i++) {
        size_t len = strlen(ops[i]);
        if (strncmp(token, ops[i], len) == 0) {
            // L'argument commence par ex. ">>"
            const char *rest = token + len;  // le reste
            if (*rest == '\0') {
                return 0; 
            }
            strcpy(redir_str, ops[i]);
            strcpy(filename_str, rest);
            return 1;
        }
    }
    return 0; // pas un token collé
}

int manage_redirections(char **args, int *arg_count,
                        char **infile, char **outfile, char **errfile,
                        TypeDeRedirection *out_type, TypeDeRedirection *err_type)
{
    *infile = NULL;
    *outfile = NULL;
    *errfile = NULL;
    *out_type = REDIR_INCONNU;
    *err_type = REDIR_INCONNU;

    for (int i = 0; i < *arg_count; i++) {
        char redir_str[10];
        char filename_str[PATH_MAX];

        redir_str[0] = '\0';
        filename_str[0] = '\0';

        if (split_redirection_token(args[i], redir_str, filename_str)) {
            char *old_token = args[i];

            // On remplace args[i] par l'opérateur pur
            args[i] = strdup(redir_str);

            // On insère un nouveau token (le filename) juste après i
            (*arg_count)++;
            // Décaler la fin du tableau
            for (int k = *arg_count; k > i+1; k--) {
                args[k] = args[k-1];
            }
            // Placer le nom de fichier
            args[i+1] = strdup(filename_str);

            // Libérer l'ancien token si besoin
            free(old_token);

            // on avance i (pour ne pas re-scinder le nouveau token)
            i++;
        }
    }

    for (int i = 0; i < *arg_count; i++) {
        TypeDeRedirection redir = getTypeRed(args[i]);
        if (redir != REDIR_INCONNU) {
            // Vérif qu'il y ait un fichier après
            if (i + 1 >= *arg_count) {
                errno = EINVAL;
                perror("Redirection sans fichier");
                return 1;
            }

            char *filename = args[i + 1];
            switch (redir) {
                case REDIR_INPUT:
                    if (*infile != NULL) {
                        errno = EEXIST;
                        perror("Redirection input multiple");
                        return 1;
                    }
                    *infile = filename;
                    break;

                case REDIR_OUTPUT:
                case REDIR_FORCE_OUTPUT:
                case REDIR_APPEND_OUTPUT:
                    if (*outfile != NULL && strcmp(*outfile, filename) != 0) {
                        errno = EEXIST;
                        perror("Redirection output multiple");
                        return 1;
                    }
                    *outfile = filename;
                    *out_type = redir;
                    break;

                case REDIR_ERROR:
                case REDIR_FORCE_ERROR:
                case REDIR_APPEND_ERROR:
                    if (*errfile != NULL && strcmp(*errfile, filename) != 0) {
                        errno = EEXIST;
                        perror("Redirection error multiple");
                        return 1;
                    }
                    *errfile = filename;
                    *err_type = redir;
                    break;

                default:
                    errno = EINVAL;
                    perror("Redirection inconnue");
                    return 1;
            }

            // Supprimer les 2 tokens (redir + fichier) du tableau
            memmove(&args[i], &args[i + 2], 
                    (*arg_count - i - 2) * sizeof(char *));
            *arg_count -= 2;
            args[*arg_count] = NULL;
            i--;
        }
    }

    return 0;
}

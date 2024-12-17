#include "../include/fsh.h"
#include "../include/commandes_internes.h"
#include "../include/commandes_simples.h"
#include "../include/prompt.h"
#include "../include/redirections.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

int last_exit_status = 0; // Statut de la dernière commande exécutée

// Analyse les redirections dans la ligne de commande sans les appliquer immédiatement.
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
            // On attend un fichier après le token de redirection
            if (i + 1 >= *arg_count) {
                fprintf(stderr, "Redirection sans fichier\n");
                return 1;
            }

            char *filename = args[i + 1];

            switch (redir) {
                case REDIR_INPUT:
                    if (*infile != NULL) {
                        fprintf(stderr, "Redirections multiples input pas supportées\n");
                        return 1;
                    }
                    *infile = filename;
                    break;

                case REDIR_OUTPUT:
                case REDIR_FORCE_OUTPUT:
                case REDIR_APPEND_OUTPUT:
                    if (*outfile != NULL) {
                        fprintf(stderr, "Redirections multiples output pas supportées\n");
                        return 1;
                    }
                    *outfile = filename;
                    *out_type = redir;
                    break;

                case REDIR_ERROR:
                case REDIR_FORCE_ERROR:
                case REDIR_APPEND_ERROR:
                    if (*errfile != NULL) {
                        fprintf(stderr, "Redirections multiples error pas supportées\n");
                        return 1;
                    }
                    *errfile = filename;
                    *err_type = redir;
                    break;

                default:
                    fprintf(stderr, "Redirection inconnu\n");
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


// Exécute une commande simple (interne ou externe), avec redirections.
static void run_simple_command(char **args, int arg_count,
                               char *infile, char *outfile, char *errfile,
                               TypeDeRedirection out_type, TypeDeRedirection err_type) {
    if (args[0] == NULL) return;
    bool is_internal = false;

    if (strcmp(args[0], "cd") == 0) {
        is_internal = true;
    } else if (strcmp(args[0], "pwd") == 0) {
        is_internal = true;
    } else if (strcmp(args[0], "exit") == 0) {
        is_internal = true;
    } else if (strcmp(args[0], "ftype") == 0) {
        is_internal = true;
    } else if (strcmp(args[0], "for") == 0) {
        is_internal = true;
    }

    if (is_internal) {
        // Sauvegarde des descripteurs
        int saved_stdin = dup(STDIN_FILENO);
        int saved_stdout = dup(STDOUT_FILENO);
        int saved_stderr = dup(STDERR_FILENO);

        // Application des redirections
        if (infile && appliqueRedirection(REDIR_INPUT, infile) != 0) {
            last_exit_status = 1;
            goto restore_fds;
        }
        if (outfile && out_type != REDIR_INCONNU) {
            if (appliqueRedirection(out_type, outfile) != 0) {
                last_exit_status = 1;
                goto restore_fds;
            }
        }
        if (errfile && err_type != REDIR_INCONNU) {
            if (appliqueRedirection(err_type, errfile) != 0) {
                last_exit_status = 1;
                goto restore_fds;
            }
        }

        // Exécuter la commande interne
        if (strcmp(args[0], "cd") == 0) {
            if (arg_count > 2) {
                fprintf(stderr, "cd: too many arguments\n");
                last_exit_status = 1;
            } else {
                const char *path = (arg_count > 1) ? args[1] : NULL;
                last_exit_status = cmd_cd(path);
            }

        } else if (strcmp(args[0], "pwd") == 0) {
            if (arg_count > 1) {
                fprintf(stderr, "pwd: %s: invalid argument\n", args[1]);
                last_exit_status = 1;
            } else {
                last_exit_status = cmd_pwd();
            }

        } else if (strcmp(args[0], "exit") == 0) {
            if (arg_count > 2) {
                fprintf(stderr, "exit: too many arguments\n");
                last_exit_status = 1;
            } else {
                int exit_val = args[1] ? atoi(args[1]) : last_exit_status;
                // Restauration avant de quitter
                dup2(saved_stdin, STDIN_FILENO);
                dup2(saved_stdout, STDOUT_FILENO);
                dup2(saved_stderr, STDERR_FILENO);
                close(saved_stdin);
                close(saved_stdout);
                close(saved_stderr);
                exit(exit_val);
            }

        } else if (strcmp(args[0], "ftype") == 0) {
            if (arg_count != 2) {
                fprintf(stderr, "Error: 'ftype' requires exactly one argument.\n");
                last_exit_status = 1;
            } else {
                last_exit_status = cmd_ftype(args[1]);
            }

        } else if (strcmp(args[0], "for") == 0) {
            last_exit_status = simple_for_loop(args);
        } else {
            last_exit_status = 1;
        }

restore_fds:
        // Restauration des descripteurs
        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stdin);
        close(saved_stdout);
        close(saved_stderr);

    } else {
        // Commande externe inchangée
        pid_t pid = fork();

        if (pid == 0) { // Enfant
            if (infile && appliqueRedirection(REDIR_INPUT, infile) != 0) {
                exit(1);
            }
            if (outfile && out_type != REDIR_INCONNU) {
                if (appliqueRedirection(out_type, outfile) != 0) {
                    exit(1);
                }
            }
            if (errfile && err_type != REDIR_INCONNU) {
                if (appliqueRedirection(err_type, errfile) != 0) {
                    exit(1);
                }
            }

            if (strlen(args[0]) > 2 && args[0][0] == '.' && args[0][1] == '/') {
                execv(args[0], args);
            } else {
                execvp(args[0], args);
            }
            perror("execvp");
            exit(EXIT_FAILURE);

        } else if (pid > 0) { // Parent
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                last_exit_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_exit_status = 1;
            } else {
                last_exit_status = 1;
            }
        } else {
            perror("fork");
            last_exit_status = 1;
        }
    }
}


void process_command(char *input) {
    // Première étape : tokenize par espaces seulement
    char *tokens[100];
    int token_count = 0;
    
    {
        char *saveptr;
        char *tok = strtok_r(input, " ", &saveptr);
        while (tok != NULL && token_count < 100) {
            tokens[token_count++] = tok;
            tok = strtok_r(NULL, " ", &saveptr);
        }
        tokens[token_count] = NULL;
    }

    // Maintenant, on détecte si la ligne contient un pipeline
    char *pipe_segments[100];
    int pipe_count = 0;
    {
        int start = 0;
        for (int i = 0; i < token_count; i++) {
            if (strcmp(tokens[i], "|") == 0) {
                int length = i - start;
                // Calcul de la taille
                int total_len = 0;
                for (int j = start; j < i; j++)
                    total_len += strlen(tokens[j]) + 1;
                
                char *segment = malloc(total_len + 1);
                segment[0] = '\0';
                for (int j = start; j < i; j++) {
                    strcat(segment, tokens[j]);
                    if (j < i-1) strcat(segment, " ");
                }

                pipe_segments[pipe_count++] = segment;
                start = i + 1;
            }
        }

        // Le dernier segment après le dernier '|'
        if (start < token_count) {
            int length = token_count - start;
            int total_len = 0;
            for (int j = start; j < token_count; j++)
                total_len += strlen(tokens[j]) + 1;

            char *segment = malloc(total_len + 1);
            segment[0] = '\0';
            for (int j = start; j < token_count; j++) {
                strcat(segment, tokens[j]);
                if (j < token_count - 1) strcat(segment, " ");
            }

            pipe_segments[pipe_count++] = segment;
        }
    }

    if (pipe_count > 1) {
        int pipe_fds[2];
        int prev_fd = -1;

        for (int i = 0; i < pipe_count; i++) {
            if (i < pipe_count - 1) {
                if (pipe(pipe_fds) == -1) {
                    perror("pipe");
                    last_exit_status = 1;
                    goto cleanup_pipeline;
                }
            }

            pid_t pid = fork();
            if (pid == 0) {
                // Enfant
                if (prev_fd != -1) {
                    dup2(prev_fd, STDIN_FILENO);
                    close(prev_fd);
                }
                if (i < pipe_count - 1) {
                    close(pipe_fds[0]);
                    dup2(pipe_fds[1], STDOUT_FILENO);
                    close(pipe_fds[1]);
                }

                char *cmd_args[100];
                int cmd_arg_count = 0;
                {
                    char *saveptr;
                    char *cmd_token = strtok_r(pipe_segments[i], " ", &saveptr);
                    while (cmd_token != NULL && cmd_arg_count < 100) {
                        cmd_args[cmd_arg_count++] = cmd_token;
                        cmd_token = strtok_r(NULL, " ", &saveptr);
                    }
                    cmd_args[cmd_arg_count] = NULL;
                }

                if (cmd_args[0] == NULL) exit(EXIT_FAILURE);

                execvp(cmd_args[0], cmd_args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                waitpid(pid, &last_exit_status, 0);
                if (prev_fd != -1) close(prev_fd);
                if (i < pipe_count - 1) {
                    close(pipe_fds[1]);
                    prev_fd = pipe_fds[0];
                }
            } else {
                perror("fork");
                last_exit_status = 1;
                goto cleanup_pipeline;
            }
        }

        if (prev_fd != -1) close(prev_fd);

cleanup_pipeline:
        for (int i = 0; i < pipe_count; i++)
            free(pipe_segments[i]);
        return;
    } else {
        // On tokenise à nouveau par espace pour gerer la commande simple + redirection
        char *args[100];
        int arg_count = 0;
        {
            char *saveptr;
            char *command = strtok_r(pipe_segments[0], " ", &saveptr);
            while (command != NULL && arg_count < 100) {
                args[arg_count++] = command;
                command = strtok_r(NULL, " ", &saveptr);
            }
            args[arg_count] = NULL;
        }

        char *infile = NULL;
        char *outfile = NULL;
        char *errfile = NULL;
        TypeDeRedirection out_type = REDIR_INCONNU;
        TypeDeRedirection err_type = REDIR_INCONNU;

        if (manage_redirections(args, &arg_count, &infile, &outfile, &errfile, &out_type, &err_type) != 0) {
            last_exit_status = 1;
            free(pipe_segments[0]);
            return;
        }

        run_simple_command(args, arg_count, infile, outfile, errfile, out_type, err_type);
        free(pipe_segments[0]);
    }
}


/**
 * @brief Point d'entrée du programme shell.
 */
int main() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    char *input;
    char current_dir[1024];

    while (1) {
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("Error retrieving the current directory");
            continue;
        }

        rl_outstream = stderr;
        char *prompt = update_prompt(last_exit_status, current_dir);
        input = readline(prompt);

        if (input == NULL) { // Ctrl-D
            write(STDOUT_FILENO, "\n", 1);
            cmd_exit(NULL);
            break;
        }

        if (strcmp(input, "") == 0) {
            free(input);
            continue;
        }

        add_history(input);
        process_command(input);

        free(input);
    }

    return 0;
}

#include "command_executor.h"
#include "command_parser.h"
#include "../include/commandes_internes.h"
#include "../include/commandes_simples.h"
#include "../include/for.h"
#include "../include/commandes_structurees.h"
#include "../include/redirections.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>

int last_exit_status = 0;
int if_test_mode = 0; // Indique si on exécute un TEST dans un if

int run_simple_command(char **args, int arg_count,
                       char *infile, char *outfile, char *errfile,
                       TypeDeRedirection out_type, TypeDeRedirection err_type);


void process_command(const char *input) {
    if (!input || strlen(input) == 0) return; // Aucune commande à traiter

    // Sépare la ligne en commandes par ';'
    char *commands[100];
    int command_count = split_commands(input, commands, 100);

    for (int i = 0; i < command_count; i++) {
        char *input_copy = strdup(commands[i]); // Copie sécurisée
        free(commands[i]);

        // Découpage par espaces de la sous-commande
        char *args[100];
        int arg_count = 0;
        {
            char *token = strtok(input_copy, " ");
            while (token != NULL && arg_count < 100) {
                if (strlen(token) > 0) {
                    args[arg_count++] = token;
                }
                token = strtok(NULL, " ");
            }
            args[arg_count] = NULL;
        }

        if (args[0] == NULL) {
            free(input_copy);
            continue; // Commande vide
        }

        // Détection du pipeline
        // On va détecter si '|' est présent dans args
        int pipe_positions[100]; 
        int pipe_count = 0;
        {
            for (int j = 0; j < arg_count; j++) {
                if (strcmp(args[j], "|") == 0) {
                    pipe_positions[pipe_count++] = j;
                }
            }
        }

        if (pipe_count > 0) {
            // Gestion du pipeline
            // Construire pipe_segments en fonction de pipe_positions
            char *pipe_segments[100];
            int segment_count = 0;
            {
                int start = 0;
                for (int p = 0; p < pipe_count; p++) {
                    int end = pipe_positions[p];
                    // Calculer la longueur totale pour ce segment
                    int total_len = 0;
                    for (int k = start; k < end; k++) {
                        total_len += strlen(args[k]) + 1;
                    }

                    char *segment = malloc(total_len + 1);
                    segment[0] = '\0';
                    for (int k = start; k < end; k++) {
                        strcat(segment, args[k]);
                        if (k < end - 1) strcat(segment, " ");
                    }
                    pipe_segments[segment_count++] = segment;
                    start = end + 1; // après le '|'
                }

                // Dernier segment après le dernier '|'
                if (pipe_positions[pipe_count - 1] < arg_count - 1) {
                    int end = arg_count;
                    int start_idx = pipe_positions[pipe_count - 1] + 1;
                    int total_len = 0;
                    for (int k = start_idx; k < end; k++)
                        total_len += strlen(args[k]) + 1;

                    char *segment = malloc(total_len + 1);
                    segment[0] = '\0';
                    for (int k = start_idx; k < end; k++) {
                        strcat(segment, args[k]);
                        if (k < end - 1) strcat(segment, " ");
                    }
                    pipe_segments[segment_count++] = segment;
                }
            }

            // Exécution du pipeline
            int pipe_fds[2];
            int prev_fd = -1;

            for (int seg = 0; seg < segment_count; seg++) {
                if (seg < segment_count - 1) {
                    if (pipe(pipe_fds) == -1) {
                        perror("pipe");
                        last_exit_status = 1;
                        goto cleanup_pipeline;
                    }
                }

                pid_t pid = fork();
                if (pid == 0) { // Enfant
                    if (prev_fd != -1) {
                        dup2(prev_fd, STDIN_FILENO);
                        close(prev_fd);
                    }
                    if (seg < segment_count - 1) {
                        close(pipe_fds[0]);
                        dup2(pipe_fds[1], STDOUT_FILENO);
                        close(pipe_fds[1]);
                    }

                    // Tokeniser le segment
                    char *cmd_args[100];
                    int cmd_arg_count = 0;
                    {
                        char *saveptr;
                        char *cmd_token = strtok_r(pipe_segments[seg], " ", &saveptr);
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
                } else if (pid > 0) { // Parent
                    int status;
                    waitpid(pid, &status, 0);
                    if (WIFEXITED(status)) {
                        last_exit_status = WEXITSTATUS(status);
                    } else {
                        last_exit_status = 1;
                    }

                    if (prev_fd != -1) close(prev_fd);
                    if (seg < segment_count - 1) {
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
            for (int seg = 0; seg < segment_count; seg++)
                free(pipe_segments[seg]);

            free(input_copy);
            continue;

        } else {
            // Pas de pipeline : gestion d'une commande simple
            // Gérer redirections avant d'exécuter la commande
            char *infile = NULL;
            char *outfile = NULL;
            char *errfile = NULL;
            TypeDeRedirection out_type = REDIR_INCONNU;
            TypeDeRedirection err_type = REDIR_INCONNU;

            if (manage_redirections(args, &arg_count, &infile, &outfile, &errfile, &out_type, &err_type) != 0) {
                last_exit_status = 1;
                free(input_copy);
                continue;
            }

            // Vérifier si c'est une commande interne ou externe
            if (strcmp(args[0], "cd") == 0) {
                if (arg_count > 2) {
                    perror("cd: too_many_arguments");
                    last_exit_status = 1;
                } else {
                    const char *path = (arg_count > 1) ? args[1] : NULL;
                    last_exit_status = cmd_cd(path);
                }
            } else if (strcmp(args[0], "pwd") == 0) {
                if (arg_count > 1) {
                    perror("pwd: extra: invalid_argument");
                    last_exit_status = 1;
                } else {
                    last_exit_status = cmd_pwd();
                }
            } else if (strcmp(args[0], "exit") == 0) {
                if (arg_count > 2) {
                    perror("exit: too_many_arguments_for_'exit'");
                    last_exit_status = 1;
                } else {
                    int exit_val = (arg_count > 1) ? atoi(args[1]) : last_exit_status;
                    free(input_copy);
                    cmd_exit((arg_count > 1) ? args[1] : NULL);
                    // Ne retournera pas ici
                }
            } else if (strcmp(args[0], "for") == 0) {
                last_exit_status = simple_for_loop(args);
            } else if (strcmp(args[0], "ftype") == 0) {
                if (arg_count != 2) {
                    perror("Error: 'ftype' command requires exactly one argument");
                    last_exit_status = 1;
                } else {
                    last_exit_status = cmd_ftype(args[1]);
                }
            } else if (strcmp(args[0], "if") == 0) {
                last_exit_status = cmd_if(args);
            } else {
                // Commande externe
                // si c'est un test d'un if, rediriger stdout et stderr vers /dev/null
                pid_t pid = fork();
                if (pid == 0) {
                    if (if_test_mode == 1) {
                        int fd = open("/dev/null", O_WRONLY);
                        if (fd != -1) {
                            dup2(fd, STDOUT_FILENO);
                            dup2(fd, STDERR_FILENO);
                            close(fd);
                        } else {
                            perror("open /dev/null");
                        }
                    }

                    exit(run_simple_command(args, arg_count, infile, outfile, errfile, out_type, err_type));
                } else if (pid > 0) {
                    int status;
                    waitpid(pid, &status, 0);
                    if (WIFEXITED(status)) {
                        last_exit_status = WEXITSTATUS(status);
                    } else {
                        last_exit_status = 1;
                    }
                } else {
                    perror("fork");
                    last_exit_status = 1;
                }
            }

            free(input_copy);
        }
    }
}

int run_simple_command(char **args, int arg_count,
                       char *infile, char *outfile, char *errfile,
                       TypeDeRedirection out_type, TypeDeRedirection err_type) {
    if (args[0] == NULL) {
        // Pas de commande à exécuter
        return 0;
    }

    bool is_internal = false;

    // Détecter si c'est une commande interne
    if (strcmp(args[0], "cd") == 0 || strcmp(args[0], "pwd") == 0 ||
        strcmp(args[0], "exit") == 0 || strcmp(args[0], "ftype") == 0 ||
        strcmp(args[0], "for") == 0) {
        is_internal = true;
    }

    // Sauvegarde des descripteurs standard
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

    if (is_internal) {
        // Commande interne
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

    } else {
        // Commande externe
        pid_t pid = fork();
        if (pid == 0) {
            // Processus enfant
            if (infile && appliqueRedirection(REDIR_INPUT, infile) != 0) exit(1);
            if (outfile && out_type != REDIR_INCONNU) {
                if (appliqueRedirection(out_type, outfile) != 0) exit(1);
            }
            if (errfile && err_type != REDIR_INCONNU) {
                if (appliqueRedirection(err_type, errfile) != 0) exit(1);
            }

            if (strlen(args[0]) > 2 && args[0][0] == '.' && args[0][1] == '/') {
                execv(args[0], args);
            } else {
                execvp(args[0], args);
            }
            perror("execvp");
            exit(EXIT_FAILURE);

        } else if (pid > 0) {
            // Processus parent
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                last_exit_status = WEXITSTATUS(status);
            } else {
                last_exit_status = 1;
            }
        } else {
            perror("fork");
            last_exit_status = 1;
        }
    }

restore_fds:
    dup2(saved_stdin, STDIN_FILENO); 
    dup2(saved_stdout, STDOUT_FILENO); 
    dup2(saved_stderr, STDERR_FILENO); 
    close(saved_stdin); 
    close(saved_stdout); 
    close(saved_stderr);

    return last_exit_status;
}

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
        char *token = strtok(input_copy, " ");
        while (token != NULL && arg_count < 100) {
            if (strlen(token) > 0) {
                args[arg_count++] = token;
            }
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        if (args[0] == NULL) {
            free(input_copy);
            continue; // Commande vide
        }

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

        int saved_stdin = dup(STDIN_FILENO);
        int saved_stdout = dup(STDOUT_FILENO);
        int saved_stderr = dup(STDERR_FILENO);

        // Appliquer les redirections
        if (infile && appliqueRedirection(REDIR_INPUT, infile) != 0) {
            last_exit_status = 1;
            goto restore_fds;
        }
        if (outfile && appliqueRedirection(out_type, outfile) != 0) {
            last_exit_status = 1;
            goto restore_fds;
        }
        if (errfile && appliqueRedirection(err_type, errfile) != 0) {
            last_exit_status = 1;
            goto restore_fds;
        }

        // Commandes internes
        if (strcmp(args[0], "cd") == 0) {
            if (arg_count > 2) {
                perror("cd: too many arguments");
                last_exit_status = 1;
            } else {
                const char *path = (arg_count > 1) ? args[1] : NULL;
                last_exit_status = cmd_cd(path);
            }
        } else if (strcmp(args[0], "pwd") == 0) {
            if (arg_count > 1) {
                perror("pwd: extra: invalid argument");
                last_exit_status = 1;
            } else {
                last_exit_status = cmd_pwd();
            }
        } else if (strcmp(args[0], "exit") == 0) {
            if (arg_count > 2) {
                perror("exit: too many arguments");
                last_exit_status = 1;
            } else {
                cmd_exit(arg_count > 1 ? args[1] : NULL);
            }
        } else if (strcmp(args[0], "for") == 0) {
            last_exit_status = simple_for_loop(args);
        } else if (strcmp(args[0], "ftype") == 0) {
            if (arg_count != 2) {
                perror("ftype: requires exactly one argument");
                last_exit_status = 1;
            } else {
                last_exit_status = cmd_ftype(args[1]);
            }
        } else if (strcmp(args[0], "if") == 0) {
            last_exit_status = cmd_if(args);
        } else {
            // Commandes externes
            pid_t pid = fork();
            if (pid == 0) { // Processus enfant
                if (if_test_mode == 1) { // Rediriger stdout/stderr vers /dev/null pour les tests
                    int fd = open("/dev/null", O_WRONLY);
                    if (fd != -1) {
                        dup2(fd, STDOUT_FILENO);
                        dup2(fd, STDERR_FILENO);
                        close(fd);
                    } else {
                        perror("open /dev/null");
                    }
                }
                
                

                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (pid > 0) { // Processus parent
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
        // Restaurer les descripteurs standard
        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stdin);
        close(saved_stdout);
        close(saved_stderr);
        free(input_copy);
    }
}

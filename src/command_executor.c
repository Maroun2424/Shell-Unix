#include "command_executor.h"
#include "command_parser.h" // Pour process_sequential_commands
#include "../include/commandes_internes.h"
#include "../include/commandes_simples.h"
#include "../include/for.h"
#include "../include/commandes_structurees.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

int last_exit_status = 0;
int if_test_mode = 0; // Variable globale pour indiquer qu'on exécute un TEST dans un if

void process_command(const char *input) {
    if (!input || strlen(input) == 0) return; // Aucune commande à traiter

    char *commands[100];
    int command_count = split_commands(input, commands, 100);

    for (int i = 0; i < command_count; i++) {
        char *input_copy = strdup(commands[i]); // Copie sécurisée pour strtok
        char *args[100];
        int arg_count = 0;

        // Découpage strict par espaces
        char *token = strtok(input_copy, " ");
        while (token != NULL && arg_count < 100) {
            // Pas de trim_whitespace
            if (strlen(token) > 0) {
                args[arg_count++] = token;
            }
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        if (args[0] == NULL) {
            free(input_copy);
            free(commands[i]);
            continue; // Commande vide
        }

        // Commandes internes
        if (strcmp(args[0], "cd") == 0) {
            if (arg_count > 2) {
                fprintf(stderr, "cd: too_many_arguments\n");
                last_exit_status = 1;
            } else {
                const char *path = (arg_count > 1) ? args[1] : NULL;
                last_exit_status = cmd_cd(path);
            }
        } else if (strcmp(args[0], "pwd") == 0) {
            if (arg_count > 1) {
                fprintf(stderr, "pwd: extra: invalid_argument\n");
                last_exit_status = 1;
            } else {
                last_exit_status = cmd_pwd();
            }
        } else if (strcmp(args[0], "exit") == 0) {
            if (arg_count > 2) {
                fprintf(stderr, "exit: too_many_arguments_for_'exit'\n");
                last_exit_status = 1;
            } else {
                cmd_exit(arg_count > 1 ? args[1] : NULL);
            }
        } else if (strcmp(args[0], "for") == 0) {
            last_exit_status = simple_for_loop(args);
        } else if (strcmp(args[0], "ftype") == 0) {
            if (arg_count != 2) {
                fprintf(stderr, "Error: 'ftype' command requires exactly one argument.\n");
                last_exit_status = 1;
            } else {
                last_exit_status = cmd_ftype(args[1]);
            }
        } else if (strcmp(args[0], "if") == 0) {
            last_exit_status = cmd_if(args);
        } else {
            // Commande externe
            pid_t pid = fork();
            if (pid == 0) { // Processus enfant
                // si on exécute un TEST d'un if, rediriger stdout et stderr vers /dev/null
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

                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (pid > 0) { // Processus parent
                int status;
                if (waitpid(pid, &status, 0) != -1) {
                    if (WIFEXITED(status)) {
                        last_exit_status = WEXITSTATUS(status);
                    } else if (WIFSIGNALED(status)) {
                        fprintf(stderr, "Killed by signal %d\n", WTERMSIG(status));
                        last_exit_status = 128 + WTERMSIG(status);
                    }
                } else {
                    perror("waitpid");
                    last_exit_status = 1;
                }
            } else { // Échec du fork
                perror("fork");
                last_exit_status = 1;
            }
        }

        // args[] pointe vers input_copy, pas de free(args[j]) ici
        free(input_copy);
        free(commands[i]);
    }
}

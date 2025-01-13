#include "command_executor.h"
#include "command_parser.h"
#include "../include/commandes_internes.h"
#include "../include/commandes_simples.h"
#include "../include/for.h"
#include "../include/commandes_structurees.h"
#include "../include/redirections.h"
#include "../include/fsh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/limits.h>

// Variables globales
int last_exit_status = 0;
int if_test_mode = 0; // Indique si on exécute un TEST dans un if
// extern char g_forValue[];
 
//essaie redirection test jalon2-extra

// static char *expand_dollar_f(const char *src) {
    // const char *pos = strstr(src, "$f");
    // if (!pos) {
        // return strdup(src);
    // }

    // char buffer[4096];
    // buffer[0] = '\0';

    // Partie avant "$f"
    // size_t prefix_len = (size_t)(pos - src);
    // strncat(buffer, src, prefix_len);

    // Insérer g_forValue
    // strncat(buffer, g_forValue, sizeof(buffer) - strlen(buffer) - 1);

    // Partie après "$f"
    // const char *after = pos + 2;  // skip "$f" 
    // strncat(buffer, after, sizeof(buffer) - strlen(buffer) - 1);

    // return strdup(buffer);
// }

/*void expand_variables_in_args(char **args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        char *expanded = expand_dollar_f(args[i]);
        if (strcmp(expanded, args[i]) != 0) {
            
            free(args[i]);
            args[i] = expanded;
        } else {
            free(expanded);
        }
    }
}*/

static void run_subcommand_in_child(char **args, int arg_count) {

    int saved_stdin  = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);

    // Redirections
    char *infile = NULL, *outfile = NULL, *errfile = NULL;
    TypeDeRedirection out_type = REDIR_INCONNU, err_type = REDIR_INCONNU;

    // Gérer les redirections (split token + 2e passe)
    if (manage_redirections(args, &arg_count,
                            &infile, &outfile, &errfile,
                            &out_type, &err_type) != 0) {
        fprintf(stderr, "Erreur lors de la gestion des redirections\n");
        _exit(1);
    }

    // Appliquer
    if (infile && appliqueRedirection(REDIR_INPUT, infile) != 0) {
        _exit(1);
    }
    if (outfile && appliqueRedirection(out_type, outfile) != 0) {
        _exit(1);
    }
    if (errfile && appliqueRedirection(err_type, errfile) != 0) {
        _exit(1);
    }

    // Commandes internes
    if (strcmp(args[0], "cd") == 0) {
        _exit(1); // "cd" ne se fait pas dans un sous-processus
    }
    else if (strcmp(args[0], "pwd") == 0) {
        if (arg_count > 1) {
            _exit(1);
        }
        int r = cmd_pwd();
        if (r < 0) {
            int signum = -r;
            kill(getpid(), signum);
            for (;;) pause();
        } else {
            _exit(r & 0xFF);
        }
    }
    else if (strcmp(args[0], "exit") == 0) {
        if (arg_count > 2) {
            last_exit_status = 1;
        } else {
            cmd_exit((arg_count > 1) ? args[1] : NULL);
        }
        if (last_exit_status < 0) {
            int signum = -last_exit_status;
            kill(getpid(), signum);
            for (;;) pause();
        } else {
            _exit(last_exit_status & 0xFF);
        }
    }
    else if (strcmp(args[0], "for") == 0) {
        int r = simple_for_loop(args);
        if (r < 0) {
            int signum = -r;
            kill(getpid(), signum);
            for (;;) pause();
        } else {
            _exit(r & 0xFF);
        }
    }
    else if (strcmp(args[0], "ftype") == 0) {
        if (arg_count != 2) {
            _exit(1);
        }
        int r = cmd_ftype(args[1]);
        if (r < 0) {
            int signum = -r;
            kill(getpid(), signum);
            for (;;) pause();
        } else {
            _exit(r & 0xFF);
        }
    }
    else if (strcmp(args[0], "if") == 0) {
        int r = cmd_if(args);
        if (r < 0) {
            int signum = -r;
            kill(getpid(), signum);
            for (;;) pause();
        } else {
            _exit(r & 0xFF);
        }
    }
    else {
        // Mode test pour "if"
        if (if_test_mode == 1) {
            int fd = open("/dev/null", O_WRONLY);
            if (fd != -1) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        // Sinon exécution externe
        execvp(args[0], args);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }

    // Restauration si besoin
    dup2(saved_stdin,  STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stderr, STDERR_FILENO);

    close(saved_stdin);
    close(saved_stdout);
    close(saved_stderr);
}

static void execute_single_command(char **args, int arg_count) {
    // Sauvegarde FDs
    int saved_stdin  = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);

    char *infile = NULL, *outfile = NULL, *errfile = NULL;
    TypeDeRedirection out_type = REDIR_INCONNU, err_type = REDIR_INCONNU;

    // Redirections
    if (manage_redirections(args, &arg_count,
                            &infile, &outfile, &errfile,
                            &out_type, &err_type) != 0) {
        last_exit_status = 1;
        goto restore_fds;
    }
    if (infile && appliqueRedirection(REDIR_INPUT, infile) != 0) {
        last_exit_status = 1; goto restore_fds;
    }
    if (outfile && appliqueRedirection(out_type, outfile) != 0) {
        last_exit_status = 1; goto restore_fds;
    }
    if (errfile && appliqueRedirection(err_type, errfile) != 0) {
        last_exit_status = 1; goto restore_fds;
    }

    // Commandes internes directes
    if (strcmp(args[0], "cd") == 0) {
        // exécution en local
        if (arg_count > 2) {
            last_exit_status = 1;
        } else {
            last_exit_status = cmd_cd((arg_count > 1) ? args[1] : NULL);
        }
    }
    else if (strcmp(args[0], "exit") == 0) {
        if (arg_count > 2) {
            last_exit_status = 1;
        } else {
            cmd_exit((arg_count > 1) ? args[1] : NULL);
        }
    }
    else {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            last_exit_status = 1;
        } else if (pid == 0) {
            // Fils
            struct sigaction sa = {0};
            sa.sa_handler = SIG_DFL; 
            sigaction(SIGTERM, &sa, NULL);
            sigaction(SIGINT, &sa, NULL);

            run_subcommand_in_child(args, arg_count);
        } else {
            // Père
            if (sigint_received) {
                last_exit_status = -SIGINT;
            } else {
                int status;
                waitpid(pid, &status, 0);

                if (WIFSIGNALED(status)) {
                    last_exit_status = -WTERMSIG(status);
                } else if (WIFEXITED(status)) {
                    last_exit_status = WEXITSTATUS(status);
                }
            }
        }
    }

restore_fds:
    dup2(saved_stdin,  STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stderr, STDERR_FILENO);

    close(saved_stdin);
    close(saved_stdout);
    close(saved_stderr);
}

static int split_pipeline(char **args, int arg_count, char ***segments, int max_segments) {
    int seg_count = 0;
    int start = 0;

    for (int i = 0; i < arg_count; i++) {
        if (strcmp(args[i], "|") == 0) {
            int length = i - start;
            if (length == 0) {
                return -1;
            }
            segments[seg_count] = malloc((length + 1) * sizeof(char*));
            for (int k = 0; k < length; k++) {
                segments[seg_count][k] = args[start + k];
            }
            segments[seg_count][length] = NULL;
            seg_count++;
            if (seg_count >= max_segments) break;

            start = i + 1;
        }
    }

    if (start < arg_count) {
        int length = arg_count - start;
        if (length > 0) {
            segments[seg_count] = malloc((length + 1) * sizeof(char*));
            for (int k = 0; k < length; k++) {
                segments[seg_count][k] = args[start + k];
            }
            segments[seg_count][length] = NULL;
            seg_count++;
        }
    }
    return seg_count;
}

static void execute_pipeline(char **args, int arg_count) {
    char **segments[50];
    memset(segments, 0, sizeof(segments));

    int seg_count = split_pipeline(args, arg_count, segments, 50);
    if (seg_count < 2) {
        if (seg_count < 0) {
            last_exit_status = 2;
        } else {
            execute_single_command(args, arg_count);
        }
        return;
    }

    
    int pipefd[50][2];
    for (int i = 0; i < seg_count - 1; i++) {
        if (pipe(pipefd[i]) < 0) {
            perror("pipe");
            last_exit_status = 1;
            for (int s = 0; s < seg_count; s++) {
                free(segments[s]);
            }
            return;
        }
    }

    pid_t pids[50];

    for (int i = 0; i < seg_count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            last_exit_status = 1;
            continue;
        }
        else if (pid == 0) {
            // Fils
            if (i > 0) {
                dup2(pipefd[i-1][0], STDIN_FILENO);
            }
            if (i < seg_count - 1) {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }

            // Fermeture des FDs
            for (int j = 0; j < seg_count - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            int local_argc = 0;
            while (segments[i][local_argc] != NULL) {
                local_argc++;
            }
            run_subcommand_in_child(segments[i], local_argc);
        }
        else {
            // Père
            pids[i] = pid;
        }
    }

    // Fermeture dans le père
    for (int j = 0; j < seg_count - 1; j++) {
        close(pipefd[j][0]);
        close(pipefd[j][1]);
    }

    // Attente
    for (int i = 0; i < seg_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == seg_count - 1) {
            // On récupère le code de la dernière commande
            if (WIFSIGNALED(status)) {
                last_exit_status = -WTERMSIG(status);
            } else if (WIFEXITED(status)) {
                if (last_exit_status >= 0) {
                    last_exit_status = WEXITSTATUS(status);
                }
            }
        }
    }

    for (int s = 0; s < seg_count; s++) {
        free(segments[s]);
    }
}

int process_command(const char *input) {

    if (!input || strlen(input) == 0) {
        return last_exit_status;
    }

    // On split par ; via split_commands
    char *commands[100];
    int command_count = split_commands(input, commands, 100);

    for (int i = 0; i < command_count; i++) {
        char *input_copy = strdup(commands[i]);
        free(commands[i]);

        // Découpe en tokens (espaces)
        char *args[100];
        int arg_count = 0;
        {
            char *token = strtok(input_copy, " ");
            while (token && arg_count < 99) {
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_count] = NULL;
        }

        // expand_variables_in_args(args, arg_count);

        // 2) Détection pipeline
        bool has_pipe = false;
        for (int k = 0; k < arg_count; k++) {
            if (strcmp(args[k], "|") == 0) {
                has_pipe = true;
                break;
            }
        }

        if (!sigint_received) {
            if (has_pipe) {
                execute_pipeline(args, arg_count);
            } else {
                execute_single_command(args, arg_count);
            }
        }

        free(input_copy);

        if (last_exit_status == -SIGINT) {
            break;
        }
    }
    return last_exit_status;
}

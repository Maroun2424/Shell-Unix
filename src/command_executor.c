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

// Variables globales déjà définies
int last_exit_status = 0;
int if_test_mode = 0; // Indique si on exécute un TEST dans un if

static void run_subcommand_in_child(char **args, int arg_count)
{

    char *infile = NULL;
    char *outfile = NULL;
    char *errfile = NULL;
    TypeDeRedirection out_type = REDIR_INCONNU;
    TypeDeRedirection err_type = REDIR_INCONNU;

    if (manage_redirections(args, &arg_count, &infile, &outfile, &errfile, &out_type, &err_type) != 0) {
        // Échec de la gestion des redirections
        exit(1);
    }

    // Appliquer réellement les redirections (dup2, open, etc.)
    if (infile && appliqueRedirection(REDIR_INPUT, infile) != 0) {
        exit(1);
    }
    if (outfile && appliqueRedirection(out_type, outfile) != 0) {
        exit(1);
    }
    if (errfile && appliqueRedirection(err_type, errfile) != 0) {
        exit(1);
    }

    if (strcmp(args[0], "cd") == 0) {
        // Exécution d'un "cd" dans un enfant ne change pas le répertoire du parent,
        // donc on avertit simplement l'utilisateur.
        fprintf(stderr, "cd in pipeline: ignored\n");
        exit(1);

    } else if (strcmp(args[0], "pwd") == 0) {
        if (arg_count > 1) {
            // On force une erreur si trop d'arguments
            fprintf(stderr, "pwd: extra: invalid argument\n");
            exit(1);
        }
        int r = cmd_pwd(); // écrit sur stdout
        exit(r);

    } else if (strcmp(args[0], "exit") == 0) {
        if (arg_count > 2) {
            fprintf(stderr, "exit: too many arguments\n");
            last_exit_status = 1;
        } else {
            cmd_exit((arg_count > 1) ? args[1] : NULL);
        }

    } else if (strcmp(args[0], "for") == 0) {
        // Sémantiquement étrange dans un pipeline, mais on peut l'exécuter quand même
        int r = simple_for_loop(args);
        exit(r);

    } else if (strcmp(args[0], "ftype") == 0) {
        if (arg_count != 2) {
            fprintf(stderr, "ftype: requires exactly one argument\n");
            exit(1);
        }
        int r = cmd_ftype(args[1]);
        exit(r);

    } else if (strcmp(args[0], "if") == 0) {
        int r = cmd_if(args);
        if(r < 0) raise(-r);
        exit(r);

    } else {
        // Commande externe
        if (if_test_mode == 1) {
            // Mode "test" => redirection de sortie vers /dev/null
            int fd = open("/dev/null", O_WRONLY);
            if (fd != -1) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        execvp(args[0], args);
        //if(strlen(args) > 3 && args[0] == '.' && args[1] == '/') execv(args[0], args);
        // Si on arrive ici, c'est une erreur
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}


static void execute_single_command(char **args, int arg_count) {
    // Sauvegarde des descripteurs standard du parent
    int saved_stdin  = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);

    char *infile = NULL, *outfile = NULL, *errfile = NULL;
    TypeDeRedirection out_type = REDIR_INCONNU, err_type = REDIR_INCONNU;

    if (manage_redirections(args, &arg_count, &infile, &outfile, &errfile, &out_type, &err_type) != 0) {
        // En cas d'échec (p.ex. redirection sans fichier), on règle last_exit_status = 1 et on sort
        last_exit_status = 1;
        goto restore_fds;
    }

    // Appliquer effectivement les redirections sur le shell parent
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

    if (strcmp(args[0], "cd") == 0) {
        // "cd" : on ne fork pas car on veut changer le répertoire du shell lui-même
        if (arg_count > 2) {
            fprintf(stderr, "cd: too many arguments\n");
            last_exit_status = 1;
        } else {
            const char *path = (arg_count > 1) ? args[1] : NULL;
            last_exit_status = cmd_cd(path);
        }
    }
    else if (strcmp(args[0], "exit") == 0) {
        // "exit" : idem, pas de fork => on quitte directement
        if (arg_count > 2) {
            fprintf(stderr, "exit: too many arguments\n");
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
        }
        else if (pid == 0) {
            struct sigaction sa = {0};
            sa.sa_handler = SIG_DFL;
            sigaction(SIGTERM, &sa, NULL);
            // Enfant : exécuter la commande
            run_subcommand_in_child(args, arg_count);
        }
        else {
            // Parent : vérifier les signaux et attendre

            if (sigint_received) {
                last_exit_status = -SIGINT; // SIGINT correspond à ce code
            } else {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                    last_exit_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    last_exit_status = -WTERMSIG(status);
                    if(last_exit_status == -SIGINT) sigint_received = 1;
                }
            }
        }
    }

restore_fds:
    // --- Restauration des descripteurs standard dans le parent ---
    dup2(saved_stdin,  STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stderr, STDERR_FILENO);

    close(saved_stdin);
    close(saved_stdout);
    close(saved_stderr);
}



static int split_pipeline(char **args, int arg_count, char ***segments, int max_segments)
{
    int seg_count = 0;
    int start = 0;  // index du début du segment courant

    for (int i = 0; i < arg_count; i++) {
        if (strcmp(args[i], "|") == 0) {
            // On clôt le segment courant
            int length = i - start; // nb d'arguments avant le '|'
            if (length == 0) {
                // syntaxe pipe invalide (ex: "| cmd" ou "cmd || cmd")
                return -1;
            }
            // On alloue un sous-tableau pour ce segment
            segments[seg_count] = malloc((length + 1) * sizeof(char*));
            for (int k = 0; k < length; k++) {
                segments[seg_count][k] = args[start + k];
            }
            segments[seg_count][length] = NULL;
            seg_count++;
            if (seg_count >= max_segments) break;

            start = i + 1; // le segment suivant commence après le '|'
        }
    }

    // Traite le dernier segment
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



static void execute_pipeline(char **args, int arg_count){
    // Découpe args en sous-commandes (segments)
    char **segments[50];
    memset(segments, 0, sizeof(segments));

    int seg_count = split_pipeline(args, arg_count, segments, 50);
    if (seg_count < 2) {
        // Soit erreur, soit un seul segment => on retombe sur exécution simple
        if (seg_count < 0) {
            fprintf(stderr, "Syntax error near pipe\n");
            last_exit_status = 2;
        } else {
            // exécuter en commande simple
            execute_single_command(args, arg_count);
        }
        return;
    }

    // Création des pipes
    // Il en faut (seg_count - 1)
    int pipefd[50][2];
    for (int i = 0; i < seg_count - 1; i++) {
        if (pipe(pipefd[i]) < 0) {
            perror("pipe");
            last_exit_status = 1;
            // Libérer la mémoire des segments
            for (int s = 0; s < seg_count; s++) {
                free(segments[s]);
            }
            return;
        }
    }

    // On stocke les pids dans un tableau
    pid_t pids[50];

    // On fork seg_count fois
    for (int i = 0; i < seg_count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            last_exit_status = 1;
            continue; // on peut tenter de continuer, mais c'est bancal
        }
        else if (pid == 0) {
            // Enfant
            if (i > 0) {
                dup2(pipefd[i-1][0], STDIN_FILENO);
            }
            // => si pas le dernier segment, on écrit dans le pipe courant
            if (i < seg_count - 1) {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }

            // Ferme tous les pipes dans l'enfant
            for (int j = 0; j < seg_count - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            // Exécute la commande i
            int local_argc = 0;
            while (segments[i][local_argc] != NULL) {
                local_argc++;
            }
            run_subcommand_in_child(segments[i], local_argc);
            // Ne jamais revenir
        }
        else {
            // Parent
            pids[i] = pid;
            // On ne ferme rien ici dans l'immédiat
        }
    }

    // Le parent ferme tous les fd
    for (int j = 0; j < seg_count - 1; j++) {
        close(pipefd[j][0]);
        close(pipefd[j][1]);
    }

    for (int i = 0; i < seg_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);

        if (i == seg_count - 1) {
            // On ne met à jour last_exit_status que pour le dernier segment
            if (WIFEXITED(status)) {
                last_exit_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_exit_status = -WTERMSIG(status);
            }
        }
    }

    // Libération de la mémoire
    for (int s = 0; s < seg_count; s++) {
        free(segments[s]);
    }
}


void process_command(const char *input)
{
    if (!input || strlen(input) == 0) return; 

    // Découpe par ';'
    char *commands[100];
    int command_count = split_commands(input, commands, 100);

    for (int i = 0; i < command_count; i++) {
        char *input_copy = strdup(commands[i]);
        free(commands[i]); // Libère la chaîne d'origine

        // Découpe la sous-commande par espaces
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
            continue; // Sous-commande vide
        }

        // Vérifie la présence d'un pipe
        bool has_pipe = false;
        for (int k = 0; k < arg_count; k++) {
            if (strcmp(args[k], "|") == 0) {
                has_pipe = true;
                break;
            }
        }

        if (has_pipe && sigint_received == 0) {
            execute_pipeline(args, arg_count);
            if(last_exit_status == -SIGINT && sigint_received == 0) sigint_received = 1;
        } else if(sigint_received == 0){
            execute_single_command(args, arg_count);
        }

        free(input_copy);
    }
}
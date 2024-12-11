#include "commandes_simples.h"

int execute_simple_command(char *args[], const char *input_redirection, 
                           const char *output_redirection, int append_mode) {
    pid_t pid;
    int status;

    // Création du processus enfant
    pid = fork();
    if (pid == 0) { // Processus enfant
        // Gestion de la redirection d'entrée
        if (input_redirection != NULL) {
            int fd_in = open(input_redirection, O_RDONLY);
            if (fd_in == -1) {
                perror("Erreur lors de l'ouverture du fichier d'entrée");
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }

        // Gestion de la redirection de sortie
        if (output_redirection != NULL) {
            int fd_out = open(output_redirection, 
                              append_mode ? O_WRONLY | O_CREAT | O_APPEND 
                                          : O_WRONLY | O_CREAT | O_TRUNC, 
                              0644);
            if (fd_out == -1) {
                perror("Erreur lors de l'ouverture du fichier de sortie");
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        // Exécution 
        execvp(args[0], args);
        perror("Erreur lors de l'exécution de la commande");
        exit(EXIT_FAILURE);

    } else if (pid > 0) { // Processus parent
        // Attente de la fin du processus enfant
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return -1; // Problème dans l'éxécution
        }
    } else {
        perror("Erreur lors de la création du processus");
        return -1;
    }
}

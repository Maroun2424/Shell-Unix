#include "../include/commandes_externes.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/**
 * Exécute une commande externe en utilisant fork et exec.
 * 
 * @param cmd La commande externe à exécuter.
 * @return Le code de sortie de la commande ou -1 en cas d'erreur.
 */
int execute_external_command(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {  // Processus enfant
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        perror("Execl a échoué");  // Si execl échoue
        exit(127);  // Sort avec un code d'erreur spécifique
    } else if (pid > 0) {  // Processus parent
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return -1;  // Gérer d'autres cas, comme un enfant terminé par signal
        }
    } else {
        perror("Fork a échoué");
        return -1;  // Échec de fork
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>

/**
 * @brief Teste la commande exit avec une valeur spécifique ou sans valeur.
 * 
 * Si une valeur est fournie, utilise cette valeur pour exit.
 * Si aucun argument n'est fourni, utilise last_status comme valeur de retour.
 * 
 * @param val Valeur de retour pour exit (-1 pour utiliser last_status).
 * @param last_status Dernière valeur de retour à utiliser si val est -1.
 */
void test_exit(int val, int last_status) {
    pid_t pid = fork();
    if (pid == 0) {
        printf("Test: exit avec %s\n", (val == -1) ? "dernière valeur" : "valeur spécifiée");
        exit((val == -1) ? last_status : val);
    } else {
        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status));
        int exit_status = WEXITSTATUS(status);
        printf("Résultat attendu: %d, Résultat obtenu: %d\n", (val == -1) ? last_status : val, exit_status);
        assert(exit_status == ((val == -1) ? last_status : val));
    }
}

int main() {
    // Test avec une valeur spécifique pour exit
    test_exit(0, 0);
    test_exit(1, 0);
    test_exit(42, 0);

    // Test sans valeur (utilise la dernière valeur)
    test_exit(-1, 5);

    return 0;
}

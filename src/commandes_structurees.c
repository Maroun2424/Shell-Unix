#include "commandes_structurees.h"
#include "../include/command_executor.h" // pour process_command et last_exit_status
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h> // Pour errno

/*
Syntaxe stricte:

if TEST { CMD }
ou
if TEST { CMD_1 } else { CMD_2 }

- "{", "}" et "else" doivent être des tokens séparés.
- Toute accolade collée à un autre mot provoque une erreur de syntaxe.
*/

/**
 * @brief Concatène args[start..end-1] en une seule chaîne séparée par des espaces.
 */
static char* join_args(char *args[], int start, int end) {
    int length = 0;
    for (int i = start; i < end; i++) {
        length += (int)strlen(args[i]) + 1;
    }

    char *result = malloc(length + 1);
    if (!result) return NULL;
    result[0] = '\0';

    for (int i = start; i < end; i++) {
        strcat(result, args[i]);
        if (i < end - 1) strcat(result, " ");
    }

    return result;
}

/**
 * @brief Vérifie que toutes les accolades sont des tokens isolés.
 *        Si une accolade est collée à un autre texte, retourne false.
 */
static bool check_braces_tokens(char *args[]) {
    for (int i = 0; args[i] != NULL; i++) {
        // Suppression d'éventuels '\n'
        char *nl = strchr(args[i], '\n');
        if (nl) *nl = '\0';

        // Suppression d'éventuels '\r'
        char *cr = strchr(args[i], '\r');
        if (cr) *cr = '\0';

        if (strchr(args[i], '{') || strchr(args[i], '}')) {
            // Le token contient une accolade.
            // Il doit être exactement "{" ou "}" pour être valide.
            if (!(strcmp(args[i], "{") == 0 || strcmp(args[i], "}") == 0)) {
                errno = EINVAL; // Argument invalide
                perror("Syntax error: braces must be isolated");
                return false;
            }
        }
    }
    return true;
}

int cmd_if(char *args[]) {
    // Vérifier les accolades
    if (!check_braces_tokens(args)) {
    errno = EINVAL; // Argument invalide pour une erreur de syntaxe
    perror("Syntax error: braces must be isolated");
    return 1;
    }

    // Deuxième nettoyage des tokens
    for (int j = 0; args[j] != NULL; j++) {
        char *nl = strchr(args[j], '\n');
        if (nl) *nl = '\0';
        char *cr = strchr(args[j], '\r');
        if (cr) *cr = '\0';
    }

    int i = 1; // après 'if'
    int test_start = i;
    int test_end = -1;
    int cmd1_start = -1;
    int cmd1_end = -1;
    int cmd2_start = -1;
    int cmd2_end = -1;

    // Trouver '{' pour séparer TEST
    while (args[i] != NULL) {
        if (strcmp(args[i], "{") == 0) {
            test_end = i;
            break;
        }
        i++;
    }

    if (test_end == -1) {
        errno = EINVAL;
        perror("Syntax error: missing '{' in if statement");
        return 1;
    }

    if (test_end == test_start) {
        errno = EINVAL;
        perror("Syntax error: no TEST before '{'");
        return 1;
    }

    i++; // après '{'
    cmd1_start = i;

    // Trouver '}'
    while (args[i] != NULL) {
        if (strcmp(args[i], "}") == 0) {
            cmd1_end = i;
            break;
        }
        i++;
    }

    if (cmd1_end == -1) {
        errno = EINVAL;
        perror("Syntax error: missing '}' for the if-block");
        return 1;
    }

    i++; // après '}'
    // Vérifier s'il y a un else
    if (args[i] != NULL && strcmp(args[i], "else") == 0) {
        i++;
        if (args[i] == NULL || strcmp(args[i], "{") != 0) {
            errno = EINVAL;
            perror("Syntax error: missing '{' after else");
            return 1;
        }
        i++; // après '{'
        cmd2_start = i;

        // Trouver '}' pour CMD_2
        while (args[i] != NULL) {
            if (strcmp(args[i], "}") == 0) {
                cmd2_end = i; 
                break;
            }
            i++;
        }

        if (cmd2_end == -1) {
            errno = EINVAL;
            perror("Syntax error: missing '}' for else-block");
            return 1;
        }
    }

    char *test_cmd = join_args(args, test_start, test_end);
    if (!test_cmd) {
        errno = ENOMEM;
        perror("Memory error (test_cmd)");
        return 1;
    }

    if (cmd1_end == cmd1_start) {
        errno = EINVAL;
        perror("Syntax error: empty if-block");
        free(test_cmd);
        return 1;
    }

    char *cmd1_cmd = join_args(args, cmd1_start, cmd1_end);
    if (!cmd1_cmd) {
        free(test_cmd);
        errno = ENOMEM;
        perror("Memory error (cmd1_cmd)");
        return 1;
    }

    char *cmd2_cmd = NULL;
    if (cmd2_start != -1 && cmd2_end != -1) {
        if (cmd2_end == cmd2_start) {
            errno = EINVAL;
            perror("Syntax error: empty else-block");
            free(test_cmd);
            free(cmd1_cmd);
            return 1;
        }
        cmd2_cmd = join_args(args, cmd2_start, cmd2_end);
        if (!cmd2_cmd) {
            free(test_cmd);
            free(cmd1_cmd);
            errno = ENOMEM;
            perror("Memory error (cmd2_cmd)");
            return 1;
        }
    }

    // Avant d'exécuter la commande TEST, activer if_test_mode
    if_test_mode = 1;
    process_command(test_cmd);
    if_test_mode = 0; // Revenir à la normale après le TEST
    int test_status = last_exit_status;
    int ret = 0;

    if (test_status == 0) {
        process_command(cmd1_cmd);
        ret = last_exit_status;
    } else {
        if (cmd2_cmd) {
            process_command(cmd2_cmd);
            ret = last_exit_status;
        } else {
            ret = 0;
        }
    }

    free(test_cmd);
    free(cmd1_cmd);
    if (cmd2_cmd) free(cmd2_cmd);

    last_exit_status = ret;
    return ret;
}

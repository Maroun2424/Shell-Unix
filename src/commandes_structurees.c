#include "commandes_structurees.h"
#include "../include/command_executor.h" 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <../include/fsh.h>

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

static bool check_braces_tokens(char *args[]) {
    for (int i = 0; args[i] != NULL; i++) {
        char *nl = strchr(args[i], '\n');
        if (nl) *nl = '\0';

        char *cr = strchr(args[i], '\r');
        if (cr) *cr = '\0';

        if (strchr(args[i], '{') || strchr(args[i], '}')) {
            if (!(strcmp(args[i], "{") == 0 || strcmp(args[i], "}") == 0)) {
                errno = EINVAL;
                perror("Syntax error: braces must be isolated");
                return false;
            }
        }
    }
    return true;
}

int cmd_if(char *args[])
{
    if (!check_braces_tokens(args)) {
        errno = EINVAL;
        perror("[ERROR] cmd_if: Syntax error: braces must be isolated");
        return 1;
    }

    // Nettoyage des \n et \r dans les args
    for (int j = 0; args[j] != NULL; j++) {
        char *nl = strchr(args[j], '\n');
        if (nl) *nl = '\0';
        char *cr = strchr(args[j], '\r');
        if (cr) *cr = '\0';
    }

    // 1) Extraction de la commande test
    int i = 1;
    int test_start = i;
    int test_end = -1;

    // Trouver la première accolade => fin de la commande test
    while (args[i] != NULL) {
        if (strcmp(args[i], "{") == 0) {
            test_end = i;
            break;
        }
        i++;
    }

    if (test_end == -1) {
        errno = EINVAL;
        perror("[ERROR] cmd_if: Syntax error: missing '{' in if statement");
        return 1;
    }

    if (test_end == test_start) {
        errno = EINVAL;
        perror("[ERROR] cmd_if: Syntax error: no TEST before '{'");
        return 1;
    }

    char *test_cmd = join_args(args, test_start, test_end);
    if (!test_cmd) {
        errno = ENOMEM;
        perror("[ERROR] cmd_if: Memory error (test_cmd)");
        return 1;
    }

    // 2) Extraction du bloc { ... } (commande 1, le bloc THEN)
    int cmd1_start = test_end + 1;
    int cmd1_end = -1;
    int brace_count = 1;

    int k = cmd1_start;
    for (; args[k]; k++) {
        if (strcmp(args[k], "{") == 0) {
            brace_count++;
        } else if (strcmp(args[k], "}") == 0) {
            brace_count--;
            if (brace_count == 0) {
                cmd1_end = k;
                break;
            }
        }
    }

    if (cmd1_end == -1) {
        errno = EINVAL;
        perror("[ERROR] cmd_if: Syntax error: missing '}' for the if-block");
        free(test_cmd);
        return 1;
    }

    if (cmd1_end == cmd1_start) {
        errno = EINVAL;
        perror("[ERROR] cmd_if: Syntax error: empty if-block");
        free(test_cmd);
        return 1;
    }

    char *cmd1_cmd = join_args(args, cmd1_start, cmd1_end);
    if (!cmd1_cmd) {
        free(test_cmd);
        errno = ENOMEM;
        perror("[ERROR] cmd_if: Memory error (cmd1_cmd)");
        return 1;
    }

    // 3) Extraction éventuelle du bloc ELSE
    int cmd2_start = -1, cmd2_end = -1;
    k = cmd1_end + 1;
    if (args[k] != NULL && strcmp(args[k], "else") == 0) {
        k++;

        int else_brace = -1;
        for (; args[k]; k++) {
            if (strcmp(args[k], "{") == 0) {
                else_brace = k;
                k++;
                break;
            }
        }

        if (else_brace == -1) {
            errno = EINVAL;
            perror("[ERROR] cmd_if: Syntax error: missing '{' after else");
            free(test_cmd);
            free(cmd1_cmd);
            return 1;
        }

        brace_count = 1;
        cmd2_start = k;
        for (; args[k]; k++) {
            if (strcmp(args[k], "{") == 0) {
                brace_count++;
            } else if (strcmp(args[k], "}") == 0) {
                brace_count--;
                if (brace_count == 0) {
                    cmd2_end = k;
                    break;
                }
            }
        }

        if (cmd2_end == -1) {
            errno = EINVAL;
            perror("[ERROR] cmd_if: Syntax error: missing '}' for else-block");
            free(test_cmd);
            free(cmd1_cmd);
            return 2;
        }

        if (cmd2_end == cmd2_start) {
            errno = EINVAL;
            perror("[ERROR] cmd_if: Syntax error: empty else-block");
            free(test_cmd);
            free(cmd1_cmd);
            return 2;
        }
    }

    char *cmd2_cmd = NULL;
    if (cmd2_start != -1 && cmd2_end != -1) {
        cmd2_cmd = join_args(args, cmd2_start, cmd2_end);
        if (!cmd2_cmd) {
            free(test_cmd);
            free(cmd1_cmd);
            errno = ENOMEM;
            perror("[ERROR] cmd_if: Memory error (cmd2_cmd)");
            return 1;
        }
    }

    // 4) Exécution du test (condition)
    if_test_mode = 1;
    process_command(test_cmd);
    if_test_mode = 0;

    // Si un signal a été reçu pendant le test
    if (last_exit_status < 0) {
        // On interrompt immédiatement cmd_if
        free(test_cmd);
        free(cmd1_cmd);
        if (cmd2_cmd) free(cmd2_cmd);
        return last_exit_status;
    }

    if (sigint_received) {
        last_exit_status = -SIGINT; 
    }

    int test_status = last_exit_status;

    // 5) THEN
    if (test_status == 0) {
        // Condition vraie => exécuter cmd1_cmd
        process_command(cmd1_cmd);

        // Si signal détecté dans THEN
        if (last_exit_status < 0) {
            free(test_cmd);
            free(cmd1_cmd);
            if (cmd2_cmd) free(cmd2_cmd);
            return last_exit_status;
        }

        if (sigint_received) {
            last_exit_status = -SIGINT;
        }
    }
    // 6) ELSE
    else {
        if (cmd2_cmd) {
            process_command(cmd2_cmd);

            // Si un signal détecté dans ELSE
            if (last_exit_status < 0) {
                free(test_cmd);
                free(cmd1_cmd);
                free(cmd2_cmd);
                return last_exit_status;
            }

            if (sigint_received) {
                last_exit_status = -SIGINT;
            }
        } else {
            last_exit_status = 0;
        }
    }

    // Nettoyage
    free(test_cmd);
    free(cmd1_cmd);
    if (cmd2_cmd) free(cmd2_cmd);

    // Retour final
    return last_exit_status;
}
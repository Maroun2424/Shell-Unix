#include "command_parser.h"
#include "../include/command_executor.h" // Pour utiliser `process_command`
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

int split_commands(const char *input, char **commands, int max_commands) {
    int command_count = 0;
    char current_command[1024] = "";
    int cmd_len = 0;
    bool in_quotes = false;
    int brace_count = 0; // Compteur d'accolades ouvertes

    for (const char *ptr = input; *ptr != '\0'; ptr++) {
        if (*ptr == '"') {
            // Basculer l'état des guillemets
            in_quotes = !in_quotes;
            if (cmd_len < (sizeof(current_command) - 1)) {
                current_command[cmd_len++] = *ptr;
            }
        } else if (!in_quotes && *ptr == '{') {
            // Entrée dans un bloc d'accolades
            brace_count++;
            if (cmd_len < (sizeof(current_command) - 1)) {
                current_command[cmd_len++] = *ptr;
            }
        } else if (!in_quotes && *ptr == '}') {
            // Fermeture d'un bloc d'accolades
            if (brace_count > 0) brace_count--;
            if (cmd_len < (sizeof(current_command) - 1)) {
                current_command[cmd_len++] = *ptr;
            }
        } else if (*ptr == ';' && !in_quotes && brace_count == 0) {
            // Séparateur de commande hors guillemets et hors accolades
            if (cmd_len > 0) {
                current_command[cmd_len] = '\0';
                commands[command_count] = strdup(current_command);
                if (!commands[command_count]) {
                    perror("strdup failed");
                    return -1; // Erreur d'allocation mémoire
                }
                command_count++;
                if (command_count >= max_commands) break;
                cmd_len = 0; // Réinitialiser pour la prochaine commande
            }
        } else {
            // Ajouter le caractère s'il reste de la place
            if (cmd_len < (sizeof(current_command) - 1)) {
                current_command[cmd_len++] = *ptr;
            } else {
                fprintf(stderr, "split_commands: Command too long, truncating\n");
                break;
            }
        }
    }

    // Ajouter la dernière commande si elle existe
    if (cmd_len > 0) {
        current_command[cmd_len] = '\0';
        commands[command_count] = strdup(current_command);
        if (!commands[command_count]) {
            perror("strdup failed");
            return -1;
        }
        command_count++;
    }

    return command_count;
}


void process_sequential_commands(const char *input) {
    char *commands[100]; // Tableau pour stocker les commandes
    int command_count = split_commands(input, commands, 100); // Utilise split_commands

    for (int i = 0; i < command_count; i++) {
        // Supprime les espaces autour de chaque commande
        char *cmd = commands[i];
        while (*cmd == ' ') cmd++; // Supprimer les espaces à gauche
        char *end = cmd + strlen(cmd) - 1;
        while (end > cmd && *end == ' ') *end-- = '\0'; // Supprimer les espaces à droite

        if (*cmd != '\0') {
            process_command(cmd); // Appelle `process_command` pour exécuter chaque commande
        }
        free(commands[i]); // Libère la mémoire de chaque commande
    }
}


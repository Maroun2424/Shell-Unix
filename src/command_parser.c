#include "command_parser.h"
#include "../include/command_executor.h" // Pour utiliser `process_command`
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int split_commands(const char *input, char **commands, int max_commands) {
    int command_count = 0;
    char current_command[1024] = "";
    int cmd_len = 0;
    bool in_quotes = false;

    for (const char *ptr = input; *ptr != '\0'; ptr++) {
        if (*ptr == '"') {
            in_quotes = !in_quotes; // Bascule l'état des guillemets
            current_command[cmd_len++] = *ptr; // Inclut le guillemet dans la commande
        } else if (*ptr == ';' && !in_quotes) {
            // Séparateur de commande hors guillemets
            if (cmd_len > 0) {
                current_command[cmd_len] = '\0';
                commands[command_count++] = strdup(current_command);
                if (command_count >= max_commands) break;
                cmd_len = 0; // Réinitialise pour la commande suivante
            }
        } else {
            current_command[cmd_len++] = *ptr;
        }
    }

    // Ajouter la dernière commande si elle existe
    if (cmd_len > 0) {
        current_command[cmd_len] = '\0';
        commands[command_count++] = strdup(current_command);
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


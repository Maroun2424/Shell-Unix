#include "../include/prompt.h"
#include <stdio.h>
#include <string.h>
#include "../include/fsh.h" // Pour `last_exit_status`


#define COLOR_GREEN "\001\033[32m\002"
#define COLOR_RED "\001\033[91m\002"
#define COLOR_BLUE "\001\033[34m\002"
#define COLOR_RESET "\001\033[0m\002"
#define MAX_PROMPT_LENGTH 512

/**
 * @brief Met à jour le prompt du shell.
 *
 * @param last_exit_status Dernier statut de commande.
 * @param current_dir Répertoire courant.
 * @return Chaîne représentant le prompt.
 */


char* update_prompt(int last_exit_status, const char *current_dir) {
    static char prompt[512]; // Buffer statique pour le prompt
    const char *status_color = (last_exit_status == 0) ? COLOR_GREEN : COLOR_RED;
    char status[6]; // Stocke le statut [xxx]
    char truncated_dir[256] = "";
    int max_visible_length = 33; // Longueur maximale visible du prompt
    int fixed_length = 5; // Longueur fixe pour "[xxx]$ ", sans inclure les couleurs
    int max_dir_length;

    // Construire le statut [xxx]
    int i = 0;
    status[i++] = '[';
    if (last_exit_status < 10) {
        status[i++] = '0' + last_exit_status;
    } else if (last_exit_status < 100) {
        status[i++] = '0' + (last_exit_status / 10); // Premier chiffre
        status[i++] = '0' + (last_exit_status % 10); // Deuxième chiffre
    } else {
        status[i++] = '0' + (last_exit_status / 100); // Premier chiffre
        status[i++] = '0' + ((last_exit_status / 10) % 10); // Deuxième chiffre
        status[i++] = '0' + (last_exit_status % 10); // Troisième chiffre
    }
    status[i++] = ']';
    status[i] = '\0';

    // Calculer la longueur maximale pour le répertoire
    max_dir_length = max_visible_length - strlen(status) - fixed_length;
    if (max_dir_length < 0) {
        max_dir_length = 0; // Sécurité
    }

    // Tronquer le répertoire si nécessaire
    int dir_len = strlen(current_dir);
    if (dir_len > max_dir_length) {
        // Ajout de "..." au début pour indiquer la troncature
        strcpy(truncated_dir, "...");
        strcat(truncated_dir, current_dir + (dir_len - max_dir_length + 3));
    } else {
        strcpy(truncated_dir, current_dir); // Pas besoin de tronquer
    }

    // Construire le prompt final
    i = 0;
    strcpy(prompt, status_color);      // Ajouter la couleur du statut
    strcat(prompt, status);            // Ajouter le statut
    strcat(prompt, COLOR_BLUE);        // Ajouter la couleur du répertoire
    strcat(prompt, truncated_dir);     // Ajouter le répertoire tronqué
    strcat(prompt, COLOR_RESET);       // Réinitialiser la couleur
    strcat(prompt, "$ ");              // Ajouter le symbole $

    return prompt;
}
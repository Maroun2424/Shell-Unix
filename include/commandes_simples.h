#ifndef COMMANDES_SIMPLES_H
#define COMMANDES_SIMPLES_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

/**
 * @brief Exécute une commande simple avec ou sans redirection.
 * 
 * @param args Tableau contenant les arguments de la commande.
 * @param input_redirection Chemin du fichier pour redirection de l'entrée (ou NULL).
 * @param output_redirection Chemin du fichier pour redirection de la sortie (ou NULL).
 * @param append_mode Spécification de si la redirection de sortie doit être en mode ajout.
 * @return Code de retour de la commande exécutée.
 */
int execute_simple_command(char *args[], const char *input_redirection, const char *output_redirection, int append_mode);

#endif

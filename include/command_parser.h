#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

/**
 * @brief Découpe une chaîne en sous-commandes séparées par `;` en respectant les guillemets.
 *
 * @param input La chaîne d'entrée.
 * @param commands Tableau pour stocker les sous-commandes.
 * @param max_commands Taille maximale du tableau.
 * @return Le nombre de sous-commandes extraites.
 */
int split_commands(const char *input, char **commands, int max_commands);
/**
 * @brief Analyse et exécute les commandes séparées par `;`.
 *
 * @param input La chaîne d'entrée.
 */
void process_sequential_commands(const char *input);

#endif // COMMAND_PARSER_H

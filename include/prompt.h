#ifndef PROMPT_H
#define PROMPT_H

/**
 * @brief Met à jour le prompt du shell.
 *
 * @param last_exit_status Dernier statut de commande.
 * @param current_dir Répertoire courant.
 * @return Chaîne représentant le prompt.
 */
char* update_prompt(int last_exit_status, const char *current_dir);

#endif // PROMPT_H

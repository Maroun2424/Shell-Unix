#ifndef FOR_H
#define FOR_H

/**
 * @brief Parcourt les fichiers d'un répertoire et exécute une commande sur chaque fichier.
 *
 * @param args Tableau de chaînes représentant les arguments.
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */

typedef struct {
    char *var_name;
    char *directory;
    char **command_parts;
    int command_parts_count;
    int show_hidden;
    int recursive;
    char *extension_filter;
    char *type_filter;
} for_loop_t;

int parse_for_command(char *args[], for_loop_t *loop);
int handle_for_iteration(for_loop_t *loop);
int simple_for_loop(char *args[]);

#endif // FOR_H

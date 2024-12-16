#ifndef FOR_H
#define FOR_H

#include <dirent.h>

// Structure pour les informations de boucle
typedef struct {
    char *var_name;
    char *directory;
    char **command_parts;
    int command_parts_count;
    int show_hidden;
    int recursive;
    char *extension_filter;
    char type_filter; // Nouveau : filtre par type (f, d, l, p)
} for_loop_t;

int initialize_loop(char *args[], for_loop_t *loop);
int parse_command_block(char *args[], int start_index, for_loop_t *loop);
int should_include_file(const struct dirent *entry, const for_loop_t *loop);
void generate_command_with_substitution(const char *template, const char *file_path, char *cmd_buffer, size_t buffer_size);
void process_directory(const for_loop_t *loop);
int simple_for_loop(char *args[]);

#endif // FOR_H

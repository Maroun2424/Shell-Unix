#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <linux/limits.h>
#include "../include/fsh.h"
#include "../include/commandes_internes.h"
#include "../include/commandes_simples.h"
#include "../include/command_executor.h"
#include <stdbool.h>

typedef struct {
    char *var_name;
    char *directory;
    char **command_parts;
    int command_parts_count;
    int show_hidden;
    int recursive;
    char *extension_filter;
    char type_filter;
} for_loop_t;

int initialize_loop(char *args[], for_loop_t *loop) {
    memset(loop, 0, sizeof(for_loop_t));

    if (!args[0] || strcmp(args[0], "for") != 0 || !args[1] || !args[2] || strcmp(args[2], "in") != 0 || !args[3]) {
        errno = EINVAL;
        perror("Syntax error: Invalid 'for' command");
        return -1;
    }

    loop->var_name = args[1];
    loop->directory = args[3];
    loop->show_hidden = 0;
    loop->recursive = 0;
    loop->extension_filter = NULL;
    loop->type_filter = '\0';

    // Analyse des options (ex: -A, -e, -t, -r)
    for (int i = 4; args[i] && strcmp(args[i], "{") != 0; i++) {
        if (strcmp(args[i], "-A") == 0) {
            loop->show_hidden = 1;
        } else if (strcmp(args[i], "-e") == 0 && args[i + 1]) {
            loop->extension_filter = strdup(args[++i]);
        } else if (strcmp(args[i], "-t") == 0 && args[i + 1]) {
            loop->type_filter = args[++i][0];
        } else if (strcmp(args[i], "-r") == 0) {
            loop->recursive = 1;
        } else {
            // Option non reconnue, on pourrait signaler une erreur
            errno = EINVAL;
            perror("Syntax error: Unknown option in 'for' command");
            return -1;
        }
    }
    return 0;
}

void generate_command_with_substitution(const char *template, const char *file_path, const for_loop_t *loop, char *cmd_buffer, size_t buffer_size) {
    cmd_buffer[0] = '\0';
    char var_pattern[3] = {'$', loop->var_name[0], '\0'};

    const char *start = template;
    while (*start) {
        const char *pos = strstr(start, var_pattern);
        if (pos) {
            strncat(cmd_buffer, start, pos - start);

            // Gestion de l'extension si nécessaire
            if (loop->extension_filter) {
                char base_name[PATH_MAX];
                strncpy(base_name, file_path, sizeof(base_name));
                base_name[sizeof(base_name)-1] = '\0';
                char *dot = strrchr(base_name, '.');
                if (dot) *dot = '\0';
                strncat(cmd_buffer, base_name, buffer_size - strlen(cmd_buffer) - 1);
            } else {
                strncat(cmd_buffer, file_path, buffer_size - strlen(cmd_buffer) - 1);
            }

            start = pos + strlen(var_pattern);
        } else {
            strncat(cmd_buffer, start, buffer_size - strlen(cmd_buffer) - 1);
            break;
        }
    }
}

int parse_command_block(char *args[], int start_index, for_loop_t *loop) {
    char command_buffer[1024] = "";
    int brace_count = 0;
    bool found_block_start = false;

    // On cherche d'abord un '{'
    int i = start_index;
    for (; args[i]; i++) {
        if (strcmp(args[i], "{") == 0) {
            brace_count = 1;
            found_block_start = true;
            i++;
            break;
        }
    }

    if (!found_block_start) {
        errno = EINVAL;
        perror("Syntax error: missing '{' in for command");
        return -1;
    }

    for (; args[i]; i++) {
        if (strcmp(args[i], "{") == 0) {
            brace_count++;
        } else if (strcmp(args[i], "}") == 0) {
            brace_count--;
            if (brace_count == 0) {
                // Fin du bloc
                break;
            } else {
                strncat(command_buffer, args[i], sizeof(command_buffer)-strlen(command_buffer)-1);
                strncat(command_buffer, " ", sizeof(command_buffer)-strlen(command_buffer)-1);
            }
        } else {
            // On est dans le bloc
            if (brace_count > 0) {
                strncat(command_buffer, args[i], sizeof(command_buffer)-strlen(command_buffer)-1);
                strncat(command_buffer, " ", sizeof(command_buffer)-strlen(command_buffer)-1);
            }
        }
    }

    if (brace_count != 0) {
        errno = EINVAL;
        perror("Syntax error: Unmatched '{'");
        return -1;
    }

    loop->command_parts = malloc(sizeof(char *));
    loop->command_parts[0] = strdup(command_buffer);
    loop->command_parts_count = 1;
    return 0;
}

int should_include_file(const struct dirent *entry, const for_loop_t *loop, const char *dir_path) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) return 0;

    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

    struct stat st;
    if (lstat(file_path, &st) == -1) return 0;

    if (entry->d_name[0] == '.' && !loop->show_hidden) return 0;

    if (loop->type_filter) {
        int type_match = 0;
        if (loop->type_filter == 'f' && S_ISREG(st.st_mode)) type_match = 1;
        else if (loop->type_filter == 'd' && S_ISDIR(st.st_mode)) type_match = 1;
        else if (loop->type_filter == 'l' && S_ISLNK(st.st_mode)) type_match = 1;
        if (!type_match) return 0;
    }

    if (loop->extension_filter) {
        char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot + 1, loop->extension_filter) != 0) return 0;
    }

    return 1;
}

int process_directory(const for_loop_t *loop) {
    DIR *dir = opendir(loop->directory);
    if (!dir) {
        perror("Error opening directory");
        return 1;
    }

    int result = 0; // 0 signifie succès, on gardera le max
    int max_return = 0; // On stocke le max des retours

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", loop->directory, entry->d_name);

        struct stat st;
        if (lstat(file_path, &st) == -1) continue;

        if (S_ISDIR(st.st_mode) && loop->recursive) {
            for_loop_t sub_loop = *loop;
            sub_loop.directory = file_path;
            // On traite le répertoire récursivement
            int sub_result = process_directory(&sub_loop);
            if (sub_result > max_return) max_return = sub_result;
        }

        if (should_include_file(entry, loop, loop->directory)) {
            char cmd_buffer[2048];
            generate_command_with_substitution(loop->command_parts[0], file_path, loop, cmd_buffer, sizeof(cmd_buffer));

            process_command(cmd_buffer); 
            // On récupère last_exit_status mis à jour par process_command
            if (last_exit_status > max_return) max_return = last_exit_status;
        }
    }

    closedir(dir);
    return max_return;
}

int simple_for_loop(char *args[]) {
    for_loop_t loop;

    if (initialize_loop(args, &loop) == -1) return 1;
    if (parse_command_block(args, 4, &loop) == -1) {
        free(loop.command_parts[0]);
        free(loop.command_parts);
        return 1;
    }

    int result = process_directory(&loop);

    free(loop.command_parts[0]);
    free(loop.command_parts);

    // On retourne le max des valeurs de retour
    last_exit_status = result;
    return result;
}

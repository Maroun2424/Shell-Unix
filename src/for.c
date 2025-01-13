#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>

#include "../include/fsh.h"
#include "../include/commandes_internes.h"
#include "../include/commandes_simples.h"
#include "../include/command_executor.h"

char g_forValue[PATH_MAX] = "";
typedef struct {
    char *var_name;          // Nom de variable (ex: F)
    char *directory;         // Répertoire de base (ex: dir-30)
    char **command_parts;    // Bloc de commandes { ... }
    int  command_parts_count;
    int  show_hidden;        // -A ?
    int  recursive;          // -r ?
    char *extension_filter;  // -e <EXT> ?
    char  type_filter;       // -t f|d|l ?
    int  max_parallel;       // -p N ?
} for_loop_t;


int initialize_loop(char *args[], for_loop_t *loop) {
    memset(loop, 0, sizeof(for_loop_t));

    // Vérification basique: "for <var> in <dir>"
    if (!args[0] || strcmp(args[0], "for") != 0 ||
        !args[1] || !args[2] || strcmp(args[2], "in") != 0 ||
        !args[3]) {
        errno = EINVAL;
        perror("Syntax error: Invalid 'for' command");
        return -1;
    }

    // Assignation des champs principaux
    loop->var_name       = args[1];
    loop->directory      = args[3];
    loop->show_hidden    = 0;
    loop->recursive      = 0;
    loop->extension_filter = NULL;
    loop->type_filter    = '\0';
    loop->max_parallel   = 1; // Par défaut, pas de parallélisme

    // Parcours des options éventuelles
    int i = 4;
    for (; args[i]; i++) {
        if (strcmp(args[i], "{") == 0) {
            // On atteint le bloc { ... }
            break;
        }
        else if (args[i][0] == '-') {
            if (strcmp(args[i], "-A") == 0) {
                loop->show_hidden = 1;
            } 
            else if (strcmp(args[i], "-e") == 0 && args[i + 1]) {
                loop->extension_filter = args[++i];
            } 
            else if (strcmp(args[i], "-t") == 0 && args[i + 1]) {
                // Type: f|d|l
                loop->type_filter = args[++i][0];
            } 
            else if (strcmp(args[i], "-r") == 0) {
                loop->recursive = 1;
            }
            else if (strcmp(args[i], "-p") == 0 && args[i + 1]) {
                // Parallélisme
                int val = atoi(args[++i]);
                if (val <= 0) val = 1;
                loop->max_parallel = val;
            } 
            else {
                // Option inconnue
                break;
            }
        } else {
            break;
        }
    }

    return 0;
}

int parse_command_block(char *args[], int start_index, for_loop_t *loop) {
    char command_buffer[1024];
    command_buffer[0] = '\0';

    int i = start_index;
    // Avancer jusqu’à "{" (ou fin)
    for (; args[i]; i++) {
        if (strcmp(args[i], "{") == 0) {
            i++;
            break;
        }
    }

    // Si on n’a pas trouvé "{"
    if (!args[i] && strcmp(args[i - 1], "{") != 0) {
        errno = EINVAL;
        perror("Syntax error: missing '{' in for command");
        return -1;
    }

    // On copie le contenu jusqu’à la "}" correspondante
    int brace_count = 1;
    for (; args[i]; i++) {
        if (strcmp(args[i], "{") == 0) {
            brace_count++;
            strncat(command_buffer, "{ ",
                    sizeof(command_buffer) - strlen(command_buffer) - 1);
        } else if (strcmp(args[i], "}") == 0) {
            brace_count--;
            if (brace_count == 0) {
                break; 
            } else {
                strncat(command_buffer, "} ",
                        sizeof(command_buffer) - strlen(command_buffer) - 1);
            }
        } else {
            // Ajout normal
            strncat(command_buffer, args[i],
                    sizeof(command_buffer) - strlen(command_buffer) - 1);
            strncat(command_buffer, " ",
                    sizeof(command_buffer) - strlen(command_buffer) - 1);
        }
    }

    // Vérification qu’on a bien refermé toutes les '{'
    if (brace_count != 0) {
        errno = EINVAL;
        perror("Syntax error: Unmatched '{'");
        return -1;
    }

    // On stocke la totalité dans command_parts[0]
    loop->command_parts = malloc(sizeof(char *));
    if (!loop->command_parts) {
        perror("malloc");
        return -1;
    }
    loop->command_parts[0] = strdup(command_buffer);
    loop->command_parts_count = 1;

    return 0;
}

void generate_command_with_substitution(const char *template,const char *file_path, const for_loop_t *loop, char *cmd_buffer, size_t buffer_size)
{
    cmd_buffer[0] = '\0';
    char var_pattern[3] = {'$', loop->var_name[0], '\0'};

    const char *start = template;
    while (*start) {
        // Cherche le motif dans la chaîne
        const char *pos = strstr(start, var_pattern);
        if (pos) {
            // On copie tout ce qu’il y a avant $F
            strncat(cmd_buffer, start, pos - start);

            // Si on a -e EXT, on supprime l'extension du file_path
            if (loop->extension_filter) {
                char base_name[PATH_MAX];
                strncpy(base_name, file_path, sizeof(base_name));
                base_name[sizeof(base_name) - 1] = '\0';

                char *dot = strrchr(base_name, '.');
                if (dot) {
                    *dot = '\0'; // retire l'extension
                }
                strncat(cmd_buffer, base_name,
                        buffer_size - strlen(cmd_buffer) - 1);
            } else {
                // Sinon, on copie le chemin complet
                strncat(cmd_buffer, file_path,
                        buffer_size - strlen(cmd_buffer) - 1);
            }

            // On se repositionne après $F
            start = pos + strlen(var_pattern);
        } else {
            // Plus de $F => on copie la fin
            strncat(cmd_buffer, start,
                    buffer_size - strlen(cmd_buffer) - 1);
            break;
        }
    }
}

int should_include_file(const struct dirent *entry, const for_loop_t *loop, const char *dir_path)
{
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        return 0;
    }

    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

    struct stat st;
    if (lstat(file_path, &st) == -1) {
        return 0;
    }

    // Fichiers / dossiers cachés => on n’inclut pas si -A n’est pas là
    if (!loop->show_hidden && entry->d_name[0] == '.') {
        return 0;
    }

    // Filtrage par type (-t f, -t d, -t l)
    if (loop->type_filter) {
        int type_ok = 0;
        if (loop->type_filter == 'f' && S_ISREG(st.st_mode)) {
            type_ok = 1;
        } else if (loop->type_filter == 'd' && S_ISDIR(st.st_mode)) {
            type_ok = 1;
        } else if (loop->type_filter == 'l' && S_ISLNK(st.st_mode)) {
            type_ok = 1;
        }
        if (!type_ok) {
            return 0;
        }
    }

    // Filtrage par extension (-e EXT)
    if (loop->extension_filter) {
        char *dot = strrchr(entry->d_name, '.');
        // On ne prend que si le .EXT correspond
        if (!dot || strcmp(dot + 1, loop->extension_filter) != 0) {
            return 0;
        }
    }

    return 1;
}

int process_directory(const for_loop_t *loop) {
    DIR *dir = opendir(loop->directory);
    if (!dir) {
        perror("Error opening directory");
        return 1;
    }

    int max_return = 0;
    struct dirent *entry;

    // Stocke les PID si on fait du parallélisme
    pid_t child_pids[512];
    int nb_children = 0;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s",
                 loop->directory, entry->d_name);

        struct stat st;
        if (lstat(file_path, &st) == -1) {
            continue;
        }

        // Ne pas descendre dans les dossiers cachés si -A n'est pas là
        if (!loop->show_hidden && entry->d_name[0] == '.' && S_ISDIR(st.st_mode)) {
            continue;
        }

        if (should_include_file(entry, loop, loop->directory)) {
            strncpy(g_forValue, file_path, sizeof(g_forValue));
            g_forValue[sizeof(g_forValue) - 1] = '\0';

            char cmd_buffer[2048];
            generate_command_with_substitution(loop->command_parts[0], file_path, loop, cmd_buffer, sizeof(cmd_buffer));

            if (loop->max_parallel > 1) {
                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork");
                    continue;
                }
                else if (pid == 0) {
                    // Fils : exécuter la commande
                    process_command(cmd_buffer);
                    _exit(last_exit_status & 0xFF);
                } else {
                    // Père : on stocke le pid
                    child_pids[nb_children++] = pid;
                    // Si on atteint la limite, on attend qu'un fils finisse
                    if (nb_children >= loop->max_parallel) {
                        int status;
                        pid_t finished = wait(&status);
                        nb_children--;

                        // Mise à jour du max_return
                        if (WIFEXITED(status)) {
                            int code = WEXITSTATUS(status);
                            if (code > max_return) {
                                max_return = code;
                            }
                        } else if (WIFSIGNALED(status)) {
                            int signum = WTERMSIG(status);
                            int ret = -signum;
                            if (ret < max_return) {
                                max_return = ret;
                            }
                        }
                    }
                }
            }
            else {
                // mode normal
                process_command(cmd_buffer);

                if (last_exit_status < 0) {
                    closedir(dir);
                    return last_exit_status;
                }
                if (last_exit_status > max_return) {
                    max_return = last_exit_status;
                }
            }
        }

        // Récursion si c’est un dossier + -r
        if (S_ISDIR(st.st_mode) && loop->recursive) {
            for_loop_t sub_loop = *loop;
            sub_loop.directory = file_path;

            int sub_result = process_directory(&sub_loop);
            if (sub_result < 0) {
                closedir(dir);
                return sub_result;
            }
            if (sub_result > max_return) {
                max_return = sub_result;
            }
        }
    }

    closedir(dir);

    // Attendre tous les enfants s’il en reste
    while (nb_children > 0) {
        int status;
        pid_t finished = wait(&status);
        nb_children--;

        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code > max_return) {
                max_return = code;
            }
        } else if (WIFSIGNALED(status)) {
            int signum = WTERMSIG(status);
            int ret = -signum;
            if (ret < max_return) {
                max_return = ret;
            }
        }
    }

    return max_return;
}

int simple_for_loop(char *args[]) {

    for_loop_t loop;
    if (initialize_loop(args, &loop) == -1) {
        return 1;
    }

    int start_i = 4;
    int brace_found = 0;
    for (; args[start_i]; start_i++) {
        if (strcmp(args[start_i], "{") == 0) {
            brace_found = 1;
            break;
        }
    }
    if (!brace_found) {
        errno = EINVAL;
        return 1;
    }

    if (parse_command_block(args, start_i, &loop) == -1) {
        return 1;
    }

    int result = process_directory(&loop);

    if (loop.command_parts) {
        free(loop.command_parts[0]);
        free(loop.command_parts);
    }

    last_exit_status = result;
    return last_exit_status;
}

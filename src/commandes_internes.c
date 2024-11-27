#include "../include/commandes_internes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <ctype.h>
#include <linux/limits.h> 
#include <linux/limits.h> 
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/limits.h>

#define MAX_PATH_LENGTH 1024

#ifndef O_DIRECTORY
#define O_DIRECTORY 0200000
#endif

/**
 * @brief Changes the current working directory.
 *
 * Switches the current working directory to the specified path. If the path is invalid,
 * or an error occurs, it displays an error message. If no path is provided, it defaults
 * to the HOME directory. If the path is "-", it switches to the last visited directory.
 *
 * @param path Pointer to the string containing the target directory path.
 * @return 0 on success, 1 on failure.
 */
int cmd_cd(const char *path) {
    static char previous_path[MAX_PATH_LENGTH];
    char current_path[MAX_PATH_LENGTH];

    if (getcwd(current_path, sizeof(current_path)) == NULL) {
        perror("cd: Error retrieving the current directory");
        return 1;
    }

    // Handle empty path or "cd" with no arguments
    if (path == NULL || strcmp(path, "") == 0) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(path, "-") == 0) { // Handle "cd -"
        if (strlen(previous_path) == 0) {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
        path = previous_path;
    }

    // Attempt to change the directory
    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }

    // Update the previous path
    strncpy(previous_path, current_path, sizeof(previous_path) - 1);
    previous_path[sizeof(previous_path) - 1] = '\0';

    return 0;
}

/**
 * @brief Prints the current working directory.
 *
 * Retrieves the current working directory and prints it to the standard output.
 *
 * @return 0 on success, 1 on failure.
 */
int cmd_pwd() {
    char path[MAX_PATH_LENGTH];

    if (getcwd(path, sizeof(path)) == NULL) {
        perror("pwd: Error retrieving the current directory");
        return 1;
    }

    write(STDOUT_FILENO, path, strlen(path));
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}

/**
 * @brief Trims whitespace from the start and end of a string.
 *
 * Modifies the input string to remove leading and trailing whitespace.
 *
 * @param str Pointer to the string to process (modified in place).
 */
void trim_whitespace(char *str) {
    char *end;

    // Trim leading spaces
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return;  // Empty string

    // Trim trailing spaces
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    *(end + 1) = '\0';  // Null terminate
}

/**
 * @brief Checks and displays the type of a file or directory.
 *
 * Uses lstat to retrieve information about a file or directory and prints its type.
 *
 * @param path Pointer to the string containing the file or directory path.
 * @return 0 on success, 1 on failure.
 */
int cmd_ftype(const char *path) {
    struct stat file_stat;
    char trimmed_path[MAX_PATH_LENGTH];

    // Copy the path and trim whitespace
    strncpy(trimmed_path, path, sizeof(trimmed_path));
    trim_whitespace(trimmed_path);

    // Retrieve the file or directory status
    if (lstat(trimmed_path, &file_stat) == -1) {
        perror("ftype");
        return 1;
    }

    // Determine and print the file type
    if (S_ISDIR(file_stat.st_mode)) {
        write(STDOUT_FILENO, "directory\n", 10);
    } else if (S_ISREG(file_stat.st_mode)) {
        write(STDOUT_FILENO, "regular file\n", 13);
    } else if (S_ISLNK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "symbolic link\n", 14);
    } else if (S_ISFIFO(file_stat.st_mode)) {
        write(STDOUT_FILENO, "named pipe\n", 11);
    } else if (S_ISSOCK(file_stat.st_mode)) {
        write(STDOUT_FILENO, "socket\n", 7);
    } else {
        write(STDOUT_FILENO, "other\n", 6);
    }
    return 0;
}

/**
 * @brief Executes a simple "for" loop to process files in a directory.
 *
 * Iterates through all non-hidden files in the specified directory and executes
 * a given command for each file.
 *
 * @param args Array of strings representing the command arguments.
 * @return 0 on success, -1 on failure.
 */
int simple_for_loop(char *args[]) {
    //Vérifie la validité des arguments
    if (!args || !args[1] || !args[2] || !args[3] || !args[5] || !args[6]) {
        fprintf(stderr, "Invalid arguments\n");
        return -1;
    }

    const char *directory = args[3];
    DIR *dir;
    struct dirent *entry;
    char command_buffer[1024];

    //Ouvre le répertoire
    dir = opendir(directory);
    if (dir == NULL) {
        perror("Error opening directory");
        return -1;
    }

    //Vérifie la syntaxe de la commande "for F in REP { CMD }"
    if (strcmp(args[0], "for") != 0 || strcmp(args[2], "in") != 0 || strcmp(args[4], "{") != 0) {
        fprintf(stderr, "Syntax error: Command must follow 'for F in REP { CMD }'\n");
        closedir(dir);
        return -1;
    }

    //Construit la variable (ex: "$F")
    char varname[strlen(args[1]) + 2];
    varname[0] = '\0';
    strcat(varname, "$");
    strcat(varname, args[1]);

    //parcourt les fichiers du répertoire
    while ((entry = readdir(dir)) != NULL) {
        //Ignore les fichiers spéciaux "." et ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue; 

        //Prépare la commande à exécuter
        strcpy(command_buffer, "");
        for (int i = 5; args[i] && strcmp(args[i], "}") != 0; ++i) {
            if (strcmp(args[i], varname) == 0) {  
                //Remplace $F par le chemin complet vers le fichier
                char temp[1024];
                temp[0] = '\0';
                strcat(temp, args[3]);  //ajoute le chemin du répertoire
                strcat(temp, "/");
                strcat(temp, entry->d_name);  //ajoute le nom du fichier
                strcat(command_buffer, temp);
            } else {
                //Ajoute les autres parties de la commande
                strcat(command_buffer, args[i]);
            }
            strcat(command_buffer, " "); // separation des arguments
        }

        //exécute la commande générée
        char final_command[1024];
        snprintf(final_command, sizeof(final_command), command_buffer, entry->d_name);
        process_command(final_command);
    }

    closedir(dir);
    return 0;
}

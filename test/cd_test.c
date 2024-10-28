#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <linux/limits.h>

void create_directory_if_not_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

/**
 * @brief Teste la commande `cd` seul pour s'assurer qu'elle retourne au répertoire HOME.
 */
void test_cd_to_home() {
    char *home_dir = getenv("HOME");
    assert(home_dir != NULL);  // Vérifie que la variable HOME est définie

    // Changer de répertoire avec `cd`
    chdir(home_dir);
    char current_dir[PATH_MAX];
    getcwd(current_dir, sizeof(current_dir));

    // Vérifie que le répertoire courant est bien HOME
    printf("Test: cd (seul) pour aller au HOME\n");
    printf("Résultat attendu: %s\n", home_dir);
    printf("Résultat obtenu: %s\n", current_dir);
    assert(strcmp(current_dir, home_dir) == 0);
}

/**
 * @brief Teste la commande `cd -` pour s'assurer qu'elle retourne au dernier répertoire visité.
 */
void test_cd_to_last_dir() {
    char initial_dir[PATH_MAX];
    getcwd(initial_dir, sizeof(initial_dir));  // Répertoire initial

    // Crée le répertoire temporaire /tmp/A 
    const char *tmp_subdir = "/tmp/A";
    create_directory_if_not_exists(tmp_subdir);

    // Changer de répertoire temporairement vers /tmp/A
    chdir(tmp_subdir);
    char tmp_dir[PATH_MAX];
    getcwd(tmp_dir, sizeof(tmp_dir));
    printf("Répertoire temporaire: %s\n", tmp_dir);

    // Aller dans un autre niveau de répertoire avec cd ../..
    chdir("../..");  // Remonte deux niveaux jusqu'à /
    char root_dir[PATH_MAX];
    getcwd(root_dir, sizeof(root_dir));
    printf("Répertoire racine: %s\n", root_dir);

    // Retour au répertoire /tmp/A avec `cd -`
    chdir(tmp_dir);
    char current_dir[PATH_MAX];
    getcwd(current_dir, sizeof(current_dir));

    // Vérifie que le répertoire courant est revenu à /tmp/A
    printf("Test: cd - pour retourner au dernier répertoire\n");
    printf("Résultat attendu: %s\n", tmp_dir);
    printf("Résultat obtenu: %s\n", current_dir);
    assert(strcmp(current_dir, tmp_dir) == 0);
}

int main() {
    test_cd_to_home();    // Teste cd seul pour HOME
    test_cd_to_last_dir(); // Teste cd - pour le dernier répertoire

    return 0;
}

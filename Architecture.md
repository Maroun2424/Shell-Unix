# ARCHITECTURE.md

## 1. Introduction

Ce projet implémente un **mini-shell** en C, capable d’exécuter :

- Des **commandes internes** : `cd`, `pwd`, `exit`, `ftype`, etc.
- Des **commandes externes** via `execvp`.
- Des **constructions structurées** comme la boucle `for` et la structure `if`.
- Des **redirections** (`<`, `>`, `>>`, `2>`, etc.) et des **pipelines** (`|`).
- La séparation de plusieurs commandes par le caractère `;`.

L’objectif est de proposer un fonctionnement similaire à un shell simplifié, via une **architecture modulaire**.

---

## 2. Organisation Générale

Le code est divisé en plusieurs **modules** :

- **`fsh.c`**  
  Point d’entrée du programme (`main`). Configure la gestion des signaux, lit la ligne de commande (via `readline`), puis appelle `process_command(...)`.

- **`command_executor.c`**  
  Gère l’exécution :
  1. Détection de la présence de pipelines (`|`).
  2. Gestion des redirections via `redirections.c`.
  3. Commandes internes vs. externes.
  4. Fork/execvp si nécessaire, ou exécution interne (`cd`, etc.).

- **`command_parser.c`**  
  Fonctions pour découper la ligne de commande :
  - Séparation par `;` pour obtenir plusieurs **commandes** distinctes.
  - Découpage en **tokens** (espaces, symboles spéciaux comme `<`, `>`, `|`, etc.).

- **`redirections.c`**  
  Détecte et applique les redirections (`<`, `>`, `>>`, `2>`...).  
  - Effectue une **double passe** :  
    1. Repérer d’éventuels tokens collés (ex. `>>fichier` → `>>` + `fichier`),  
    2. Associer l’opérateur et le fichier suivant, puis modifier le tableau d’arguments.

- **`for.c`**  
  Implémente la **commande structurée** `for`. Permet de parcourir un répertoire (et sous-répertoires en mode récursif) et d’exécuter un bloc de commandes pour chaque fichier correspondant.

- **`commandes_internes.c` / `commandes_structurees.c`**  
  Implémente les commandes internes (`cd`, `exit`, `pwd`, `ftype`) et les commandes structurées comme `if`.

- **`prompt.c`**  
  Gère l’affichage du prompt (couleurs, statut de la dernière commande, etc.).

---

## 3. Stratégie Générale

### 3.1 Lecture et parsing
1. **Lecture** de la ligne de commande (avec `readline`).
2. **Séparation** en **commandes** par le caractère `;`.
3. Pour chaque commande ainsi isolée, **découpage** en **tokens** (espaces, symboles spéciaux).

### 3.2 Exécution
- Si **pipeline** (`|`), on segmente la commande, on crée les tubes nécessaires et on exécute chaque segment dans un fils différent.
- Sinon, on exécute la commande (interne ou externe).
  - Les **redirections** sont appliquées avant l’exécution.
  - Les **commandes internes** sont exécutées directement (ex. `cd` modifie le répertoire du shell).
  - Les **commandes externes** font l’objet d’un `fork()` + `execvp(...)`.

### 3.3 Redirections
- Détectées par `manage_redirections(...)` :
- Appliquées via `dup2(...)` juste avant l’exécution (dans le fils ou avant `execvp`).

### 3.4 La boucle `for`

1. **Syntaxe** :  
   ```sh
   for <VAR> in <DIR> [-A] [-r] [-t TYPE] [-e EXT] [-p MAX] { ... }
2. **Parsing** : la fonction initialize_loop(...) récupère VAR, DIR, les options (-A, -r, etc.) et stocke cela dans une structure for_loop_t.

**Bloc de commandes** (entre { ... }) : placé dans command_parts[0].

**Parcours** du répertoire (process_directory(...)) :

- Ouvre le répertoire (opendir) et lit chaque entrée (readdir).

- Applique un filtrage selon l’option -A (fichiers cachés), -r (récursif), -t f|d|l, -e EXT, etc.
- Pour chaque fichier retenu :
    - On appelle process_command(...) sur cette nouvelle commande.

- S’il y a -p N, on exécute jusqu’à N occurrences en parallèle (on fork N fils). Lorsque l’un se termine, on peut en lancer un autre.

**Récursion (-r)** : on rappelle process_directory(...) si l’entrée est un répertoire.

## 3.5 La structure if

### Syntaxe (simplifiée) :

```sh
if <TEST> { ... } [ else { ... } ]
```

- On parse le `TEST` (tout ce qui se trouve avant la première accolade).
- `if_test_mode` peut être activé pour exécuter la commande `TEST` en mode silencieux ou partiellement redirigé.
- Le code de retour détermine si on exécute le bloc `{ ... }` suivant ou le bloc `else`.

### Blocs :

- Chaque bloc entre `{ ... }` est stocké comme une unique chaîne de commandes dans `command_parts[]`.
- On exécute le bloc `then` si le `TEST` retourne 0, sinon le bloc `else` si présent.

## 4. Structures de Données

- **Tableaux de `char *`** pour la liste d’arguments.
- **`for_loop_t`** : stocke toutes les informations de la boucle (var_name, directory, options...).
- **`TypeDeRedirection`** : enum répertoriant `REDIR_INPUT`, `REDIR_OUTPUT`, `REDIR_APPEND_OUTPUT`, etc.

## 5. Conclusion

### Cette architecture modulaire facilite :

- La gestion des redirections (uniquement dans le père, avant le fork),
- L’exécution (interne / externe / pipeline) via `command_executor.c`,
- L’extension aux constructions avancées (`for`, `if`) via des fichiers dédiés,
- La séparation de plusieurs commandes par `;`, gérées au niveau du parsing.

### La boucle `for` s’appuie essentiellement sur :

- Un filtre (type / extension / caché),
- L’appel récurrent à `process_command` pour exécuter les commandes sur chaque occurrence.

### La structure `if` permet :

- Un test (commande quelconque, code de retour 0 ou pas),
- Un bloc `then` (`{ ... }`) exécuté si le test est valide,
- Un bloc `else` optionnel (`{ ... }`) exécuté sinon.

Ainsi, notre mini-shell peut traiter des scénarios relativement complexes (redirections, pipelines, boucles `for`, structures conditionnelles `if`, etc.), tout en conservant une organisation claire et évolutive.

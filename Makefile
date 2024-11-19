# Définition des variables
CC = gcc
CFLAGS = -I./include -Wall -Wextra
SRC = src/fsh.c src/commandes_internes.c src/commandes_externes.c
OBJ = $(SRC:src/%.c=bin/%.o)
TARGET = bin/fsh

# Création du dossier bin si nécessaire
$(shell mkdir -p bin)

# Règle par défaut pour construire l'exécutable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) -lreadline

# Règle pour compiler les fichiers objets dans le dossier bin
bin/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyer les fichiers compilés
clean:
	rm -f bin/*.o $(TARGET)
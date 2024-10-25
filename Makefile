# Définition des variables
CC = gcc
CFLAGS = -I./include -Wall -Wextra
SRC = src/fsh.c src/commands.c
OBJ = $(SRC:.c=.o)
TARGET = fsh

# Règle par défaut pour construire l'exécutable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) -lreadline

# Règle pour compiler les fichiers objets
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyer les fichiers compilés
clean:
	rm -f $(OBJ) $(TARGET)

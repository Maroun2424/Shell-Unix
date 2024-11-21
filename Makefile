# Définition des variables
CC = gcc
CFLAGS = -I./include -Wall -Wextra
SRCDIR = src
BINDIR = .bin
TARGET = fsh

# Liste des fichiers sources et transformation en fichiers objets dans le répertoire .bin
SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(SRC:$(SRCDIR)/%.c=$(BINDIR)/%.o)

# Règle par défaut pour construire l'exécutable
$(TARGET): $(OBJ)
	$(CC) $^ -o $@ -lreadline

# Règle pour compiler les fichiers objets et les placer dans .bin
$(BINDIR)/%.o: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Créer le répertoire .bin si nécessaire
$(BINDIR):
	mkdir -p $(BINDIR)

# Nettoyer les fichiers compilés
clean:
	rm -f $(BINDIR)/*.o $(TARGET)
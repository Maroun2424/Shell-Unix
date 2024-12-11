# Définition des variables
CC = gcc
CFLAGS = -I./include -Wall -Wextra
SRCDIR = src
BINDIR = .bin
TARGET = fsh

# Liste des fichiers sources et transformation en fichiers objets dans le répertoire .bin
SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(patsubst $(SRCDIR)/%.c, $(BINDIR)/%.o, $(SRC))

# Règle par défaut pour construire l'exécutable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) -lreadline

# Règle pour compiler les fichiers objets et les placer dans .bin
$(BINDIR)/%.o: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Créer le répertoire .bin si nécessaire
$(BINDIR):
	mkdir -p $(BINDIR)

# Nettoyer les fichiers compilés
clean:
	rm -f $(BINDIR)/*.o $(TARGET)

# Ajouter une règle pour tout nettoyer (objets, exécutable et répertoire .bin)
distclean: clean
	rm -rf $(BINDIR)

# Afficher les fichiers objets pour vérifier la correspondance
print:
	@echo "Fichiers sources: $(SRC)"
	@echo "Fichiers objets: $(OBJ)"

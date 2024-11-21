#!/bin/bash

# Lancez votre shell personnalisé
./fsh << 'EOF'
cd /  # Changez au répertoire racine
pwd   # Doit afficher "/"
cd invalidpath  # Tentez de changer vers un chemin non valide
pwd   # Doit toujours afficher "/" car le changement a échoué
cd /usr
pwd   # Doit afficher "/usr"
EOF

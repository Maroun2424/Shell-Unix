#!/bin/bash

# Vérifie qu'un argument est passé
if [ $# -ne 1 ]; then
    echo "Usage: $0 <exit_code>" >&2
    exit 1
fi

# Vérifie que l'argument est un entier
if ! [[ $1 =~ ^-?[0-9]+$ ]]; then
    echo "Error: <exit_code> must be an integer" >&2
    exit 1
fi

# Limite le code de retour à la plage 0-255
exit $(( $1 & 255 ))

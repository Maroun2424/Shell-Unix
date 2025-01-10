#ifndef FSH_H
#define FSH_H

#include <signal.h>

// Déclare sigint_received comme externe (définie ailleurs)
extern volatile sig_atomic_t sigint_received;

// Déclaration des autres variables ou fonctions
extern int last_exit_status;

#endif // FSH_H


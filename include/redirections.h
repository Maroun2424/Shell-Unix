#ifndef REDIRECTIONS_H
#define REDIRECTIONS_H

#include <string.h>

typedef enum {
    REDIR_INCONNU, // inconnu
    REDIR_INPUT, // (<)
    REDIR_OUTPUT, // (>)
    REDIR_FORCE_OUTPUT, // (>|)
    REDIR_APPEND_OUTPUT, // (>>)
    REDIR_ERROR, // (2>)
    REDIR_FORCE_ERROR, // (2>|)
    REDIR_APPEND_ERROR // (2>>)
} TypeDeRedirection;

TypeDeRedirection getTypeRed(const char *token);
int appliqueRedirection(TypeDeRedirection type, const char *filename);

#endif // REDIRECTIONS_H

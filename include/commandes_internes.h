#ifndef COMMANDES_INTERNES_H
#define COMMANDES_INTERNES_H

int cmd_cd(const char *path);
void cmd_exit(const char *exit_code_str);
int cmd_pwd();
int cmd_ftype(const char *path);

#endif // COMMANDES_INTERNES_H

#ifndef COMMANDS_H
#define COMMANDS_H

void cmd_cd(const char *path);
int cmd_pwd();  // Modifiez cela si nécessaire
void cmd_exit(int exit_code);
void cmd_ftype(const char *path);

#endif // COMMANDS_H

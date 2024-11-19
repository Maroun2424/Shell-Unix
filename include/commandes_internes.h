#ifndef COMMANDS_H
#define COMMANDS_H

int cmd_cd(const char *path);
int cmd_pwd();  
//void cmd_exit(int exit_code);
int cmd_ftype(const char *path);

#endif
#ifndef COMMAND_H
#define COMMAND_H

struct token;

int is_executable(char *path); // its in locate.c

void build_argv(struct token *current,char **argv,int *argc);
void run_command(char *path,char **argv);
int find_path(char *filename,char *result)

void exec_command(struct token *current);

#endif
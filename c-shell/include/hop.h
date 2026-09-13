#ifndef HOP_H
#define HOP_H

#include <limits.h>

extern char home_Dir[PATH_MAX];
extern char prev_dir[PATH_MAX];
extern int prev_dir_flag;

struct token;
void init_hop(char *home_dir);
int change_dir(char *path);
void exec_hop(struct token *current);

#endif
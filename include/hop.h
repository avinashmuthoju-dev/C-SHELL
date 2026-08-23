#ifndef HOP_H
#define HOP_H


extern char home_Dir[1024];
extern char prev_dir[1024];
extern int prev_dir_flag;

struct token;
void init_hop(char *home_dir);
int change_dir(char *path);
void exec_hop(struct token *current);

#endif
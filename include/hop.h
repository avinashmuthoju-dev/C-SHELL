#ifndef HOP_H
#define HOP_H

struct token;
void init_hop(char *home_dir);
int change_dir(char *path);
void exec_hop(struct token *current);

#endif
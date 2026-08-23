#ifndef REVEAL_H
#define REVEAL_H

struct token;

void reveal_directory(char *path, char *display_path, int show_all);
void exec_reveal(struct token *current);

#endif
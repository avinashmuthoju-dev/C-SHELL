#ifndef LOCATE_H
#define LOCATE_H

struct token;

int is_executable(char *path);
int search_path(char *filename);
int search_current_directory(char *filename);
void exec_locate(struct token *current);

#endif
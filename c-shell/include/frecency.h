#ifndef FRECENCY_H
#define FRECENCY_H

void init_frecency();
void record_visit(char *path);
int find_frecency_match(char *name,char *result);

#endif
#ifndef FRECENCY_H
#define FRECENCY_H

void init_frecency(void);
void record_visit(const char *path);
int find_frecency_match(const char *name, char *result);

#endif
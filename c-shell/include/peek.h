#ifndef PEEK_H
#define PEEK_H

struct token;
int is_empty(char *line);
int get_line_count(FILE *fp);
void nums_line(FILE *fp,int *count);
void reverse(FILE *fp);
void reverse_with_nums(FILE *fp,int total_lines);
void exec_peak(struct token *current);

#endif
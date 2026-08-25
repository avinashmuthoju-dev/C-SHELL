#ifndef COMMAND_H
#define COMMAND_H

struct token;

struct output_file {
    char *name;
    int append;
};

int is_executable(char *path); // its in locate.c

int parse_input_redirect(struct token *current,char **argv,int *argc,char **files,int *file_count);
int parse_output_redirection(char **argv,int *argc,struct output_file *files,int *file_count);
int open_output_files(struct output_file *files,int file_count,int *fds);
int find_path(char *filename,char *result);
void run_command_with_output(char *path,char **argv,char **input_files,int input_file_count,struct output_file *files,int file_count);
int setup_input(char **files,int file_count,pid_t *writer_pid);

void exec_command(struct token *current);

#endif
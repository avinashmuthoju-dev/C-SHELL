#ifndef COMMAND_H
#define COMMAND_H

#include <sys/types.h>

struct token;

struct output_file {
    char *name;
    int append;
};

struct pipeline_stage {
    char *argv[100];
    int argc;
    char *input_files[100];
    int input_count;
    struct output_file output_files[100];
    int output_count;
    int output_fds[100];
    int output_pipe[2];
    pid_t output_writer;
    int input_fd;
    pid_t input_writer;
    int setup_failed;
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
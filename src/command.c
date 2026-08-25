#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#include <fcntl.h>

#include "command.h"
#include "lexer.h"

void build_argv(struct token *current,char **argv,int *argc){
    *argc=0;

    while(current!=NULL){
        argv[(*argc)++]=current->value;
        current=current->next;
    }
    argv[*argc]=NULL;
}

void run_command(char *path, char **argv,
                 char **files, int file_count)
{
    int fds[100];

    /* First open ALL files */
    for(int i = 0; i < file_count; i++){

        fds[i] = open(files[i], O_RDONLY);

        if(fds[i] == -1){

            printf("cshell: no such file or directory\n");

            /* Close files already opened */
            for(int j = 0; j < i; j++){
                close(fds[j]);
            }

            return;
        }
    }

    int pipefd[2];

    if(pipe(pipefd) == -1){
        for(int i = 0; i < file_count; i++){
            close(fds[i]);
        }
        return;
    }

    pid_t pid = fork();

    if(pid == -1){
        close(pipefd[0]);
        close(pipefd[1]);

        for(int i = 0; i < file_count; i++){
            close(fds[i]);
        }

        return;
    }

    if(pid == 0){

        /* Child */

        close(pipefd[1]);

        if(file_count > 0){

            if(dup2(pipefd[0], STDIN_FILENO) == -1){
                exit(1);
            }
        }

        close(pipefd[0]);

        for(int i = 0; i < file_count; i++){
            close(fds[i]);
        }

        execv(path, argv);

        exit(1);
    }

    /* Parent */

    close(pipefd[0]);

    for(int i = 0; i < file_count; i++){

        char buffer[1024];
        ssize_t bytes;

        while((bytes = read(fds[i],
                            buffer,
                            sizeof(buffer))) > 0){

            write(pipefd[1], buffer, bytes);
        }

        close(fds[i]);
    }

    close(pipefd[1]);

    waitpid(pid, NULL, 0);
}

int find_path(char *filename,char *result){
    char *env_path=getenv("PATH");

    if(env_path==NULL){
        return 0;
    }
    char path_copy[4096];
    strcpy(path_copy,env_path);

    char *dir=strtok(path_copy,":");

    while(dir!=NULL){
        snprintf(result,4096,"%s/%s",dir,filename);
        if(is_executable(result)){
            return 1;
        }

        dir=strtok(NULL,":");
    }
    return 0;
}

void parse_input_redirect(struct token *current,
                          char **argv,
                          int *argc,
                          char **files,
                          int *file_count)
{
    *argc = 0;
    *file_count = 0;

    while(current != NULL){

        if(strcmp(current->value, "<") == 0){

            current = current->next;

            if(current != NULL){
                files[(*file_count)++] = current->value;
            }
        }
        else{
            argv[(*argc)++] = current->value;
        }

        current = current->next;
    }

    argv[*argc] = NULL;
}

int setup_input(char **files, int file_count)
{
    int pipefd[2];

    if(pipe(pipefd) == -1){
        return -1;
    }

    for(int i = 0; i < file_count; i++){

        int fd = open(files[i], O_RDONLY);

        if(fd == -1){
            close(pipefd[0]);
            close(pipefd[1]);
            return -1;
        }

        char buffer[1024];
        ssize_t bytes;

        while((bytes = read(fd, buffer, sizeof(buffer))) > 0){
            write(pipefd[1], buffer, bytes);
        }

        close(fd);
    }

    close(pipefd[1]);

    return pipefd[0];
}

void exec_command(struct token *current)
{
    char *argv[100];
    char *files[100];

    int argc;
    int file_count;

    parse_input_redirect(current,
                         argv,
                         &argc,
                         files,
                         &file_count);

    char *command = argv[0];
    char path[4096];

    if(strchr(command, '/') != NULL){

        if(is_executable(command)){
            run_command(command, argv, files, file_count);
        }
        else{
            printf("cshell: command not found (%s)\n", command);
        }
    }
    else if(command[0] == '%'){

        char *name = command + 1;

        if(find_path(name, path)){
            argv[0] = name;
            run_command(path, argv, files, file_count);
        }
        else{
            printf("cshell: command not found (%s)\n", name);
        }
    }
    else{

        snprintf(path, sizeof(path), "./%s", command);

        if(is_executable(path)){
            run_command(path, argv, files, file_count);
        }
        else if(find_path(command, path)){
            run_command(path, argv, files, file_count);
        }
        else{
            printf("cshell: command not found (%s)\n", command);
        }
    }
}
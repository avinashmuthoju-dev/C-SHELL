#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/types.h>

#include "command.h"
#include "lexer.h"


int setup_input(char **files, int file_count, pid_t *writer_pid);

int parse_input_redirect(struct token *current,char **argv,int *argc,char **files,int *file_count){
    *argc = 0;
    *file_count = 0;

    while(current != NULL){
        if(strcmp(current->value, ";") == 0 ||strcmp(current->value, "&") == 0){
            break;
        }

        if(strcmp(current->value, "<") == 0){
            current = current->next;
            if(current == NULL || current->type != WORD){
                return 0;
            }
            files[(*file_count)++] = current->value;
        }
        else{
            argv[(*argc)++] = current->value;
        }
        current = current->next;
    }
    argv[*argc] = NULL;
    return 1;
}

int parse_output_redirection(char **argv,int *argc,struct output_file *files,int *file_count){
    int new_argc = 0;
    int i = 0;

    *file_count = 0;

    while(i<*argc){
        if(strcmp(argv[i], ">") == 0 ||strcmp(argv[i], ">>") == 0){
            int append = 0;

            if(strcmp(argv[i], ">>") == 0) {
                append = 1;
            }
            i++;
            if(i>=*argc){
                return 0;
            }

            files[*file_count].name = argv[i];
            files[*file_count].append = append;
            (*file_count)++;

            i++;
        }
        else{
            argv[new_argc++] = argv[i];
            i++;
        }
    }

    argv[new_argc] = NULL;
    *argc = new_argc;
    return 1;
}

int open_output_files(struct output_file *files,int file_count,int *fds){
    for (int i = 0; i < file_count; i++) {

        int flags = O_WRONLY | O_CREAT;

        if (files[i].append) {
            flags |= O_APPEND;
        }
        else {
            flags |= O_TRUNC;
        }

        fds[i] = open(files[i].name, flags, 0644);
        if (fds[i] == -1) {
            printf("cshell: unable to create file for writing\n");
            for (int j = 0; j < i; j++) {
                close(fds[j]);
            }
            return 0;
        }
    }

    return 1;
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

void run_command_with_output(char *path,char **argv,char **input_files,int input_file_count,struct output_file *files,int file_count){
    int fds[100];
    int input_fd = -1;
    pid_t writer_pid = -1;
    if (input_file_count > 0) {
        input_fd = setup_input(input_files, input_file_count, &writer_pid);
        if (input_fd == -1) {
            printf("cshell: no such file or directory\n");
            return;
        }
    }
    if (!open_output_files(files, file_count, fds)) {
        if (input_fd != -1) {
            close(input_fd);
        }
        if (writer_pid != -1) {
            waitpid(writer_pid, NULL, 0);
        }
        return;
    }

    int pipefd[2];
    if(file_count > 0 && pipe(pipefd) == -1) {
        if (input_fd != -1) {
            close(input_fd);
        }
        if (writer_pid != -1) {
            waitpid(writer_pid, NULL, 0);
        }
        for (int i = 0; i < file_count; i++) {
            close(fds[i]);
        }
        return;
    }
    pid_t pid = fork();

    if(pid==-1){
        if (file_count > 0) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        if (input_fd != -1) {
            close(input_fd);
        }
        if (writer_pid != -1) {
            waitpid(writer_pid, NULL, 0);
        }
        for (int i = 0; i < file_count; i++) {
            close(fds[i]);
        }
        return;
    }

    if(pid==0){
        if (input_fd != -1) {
            if (dup2(input_fd, STDIN_FILENO) == -1) {
                exit(1);
            }
            close(input_fd);
        }
        if (file_count > 0) {
            close(pipefd[0]);
            if(dup2(pipefd[1],STDOUT_FILENO) == -1){
                exit(1);
            }

            close(pipefd[1]);
        }
        execv(path, argv);
        exit(1);
    }

    if(input_fd!=-1){
        close(input_fd);
    }

    if(file_count>0){
        close(pipefd[1]);
        char buffer[4096];
        ssize_t bytes;

        while((bytes=read(pipefd[0],buffer,sizeof(buffer)))>0){
            for(int i=0;i<file_count;i++){
                ssize_t written=0;
                while(written<bytes){
                    ssize_t result=write(fds[i],buffer+written,(size_t)(bytes-written));
                    if(result<=0){
                        break;
                    }
                    written+=result;
                }
            }
        }
        close(pipefd[0]);
    }

    for(int i=0;i<file_count;i++){
        close(fds[i]);
    }

    waitpid(pid,NULL,0);
    if(writer_pid!=-1){
        waitpid(writer_pid,NULL,0);
    }
}

int setup_input(char **files,int file_count,pid_t *writer_pid){
    int input_fds[100];
    int pipefd[2];

    for(int i=0;i<file_count;i++){
        input_fds[i]=open(files[i],O_RDONLY);
        if(input_fds[i]==-1){
            for(int j=0;j<i;j++){
                close(input_fds[j]);
            }
            return -1;
        }
    }

    if(pipe(pipefd)==-1){
        for(int i=0;i<file_count;i++){
            close(input_fds[i]);
        }
        return -1;
    }

    *writer_pid=fork();
    if(*writer_pid==-1){
        close(pipefd[0]);
        close(pipefd[1]);
        for(int i=0;i<file_count;i++){
            close(input_fds[i]);
        }
        return -1;
    }

    if(*writer_pid==0){
        close(pipefd[0]);
        for(int i=0;i<file_count;i++){
            char buffer[4096];
            ssize_t bytes;

            while((bytes=read(input_fds[i],buffer,sizeof(buffer)))>0){
                ssize_t written=0;
                while(written<bytes){
                    ssize_t result=write(pipefd[1],buffer+written,(size_t)(bytes-written));
                    if(result<=0){
                        close(input_fds[i]);
                        close(pipefd[1]);
                        _exit(1);
                    }
                    written+=result;
                }
            }
            close(input_fds[i]);
        }
        close(pipefd[1]);
        _exit(0);
    }

    close(pipefd[1]);
    for(int i=0;i<file_count;i++){
        close(input_fds[i]);
    }
    return pipefd[0];
}

void exec_command(struct token *current){
    char *argv[100];
    int argc;

    char *input_files[100];
    int input_file_count;

    struct output_file files[100];
    int file_count;

    if(!parse_input_redirect(current, argv, &argc, input_files,&input_file_count)){
        printf("cshell: invalid syntax\n");
        return;
    }
    if(!parse_output_redirection(argv,&argc,files,&file_count)){
        return;
    }
    if(argc==0){
        return;
    }
    char *command=argv[0];
    char path[4096];

    if(strchr(command,'/')!=NULL){
        if(is_executable(command)){
            run_command_with_output(command,argv,input_files,input_file_count,files,file_count);
        }
        else{
            printf("cshell: command not found (%s)\n", command);
        }
    }
    else if(command[0]=='%'){
        char *name = command + 1;
        if(find_path(name,path)){
            argv[0] = name;
            run_command_with_output(path,argv,input_files,input_file_count,files,file_count);
        }
        else{
            printf("cshell: command not found (%s)\n", name);
        }
    }
    else{
        snprintf(path,sizeof(path),"./%s",command);

        if(is_executable(path)){
            run_command_with_output(path,argv,input_files,input_file_count,files,file_count);
        }
        else if(find_path(command,path)){
            run_command_with_output(path,argv,input_files,input_file_count,files,file_count);
        }
        else {
            printf("cshell: command not found (%s)\n", command);
        }
    }
}
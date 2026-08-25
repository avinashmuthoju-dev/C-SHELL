#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/types.h>
#include<limits.h>

#include "command.h"
#include "lexer.h"


int setup_input(char **files, int file_count, pid_t *writer_pid);

int parse_output_redirection(char **argv,int *argc,struct output_file *files,int *file_count);

int parse_pipeline_stage(struct token *start,struct token *end,struct pipeline_stage *stage){
    char *raw_argv[100];
    int raw_argc = 0;
    struct token *current = start;

    stage->argc = 0;
    stage->input_count = 0;
    stage->output_count = 0;
    stage->input_fd = -1;
    stage->input_writer = -1;
    stage->output_pipe[0] = -1;
    stage->output_pipe[1] = -1;
    stage->output_writer = -1;
    stage->setup_failed = 0;

    while (current != end) {
        if (strcmp(current->value, "<") == 0) {
            current = current->next;
            if (current == end || current == NULL || current->type != WORD) {
                return 0;
            }
            stage->input_files[stage->input_count++] = current->value;
        }
        else {
            raw_argv[raw_argc++] = current->value;
        }
        current = current->next;
    }
    raw_argv[raw_argc] = NULL;

    if (!parse_output_redirection(raw_argv, &raw_argc,stage->output_files,&stage->output_count)) {
        return 0;
    }
    for (int i = 0; i < raw_argc; i++) {
        stage->argv[i] = raw_argv[i];
    }
    stage->argv[raw_argc] = NULL;
    stage->argc = raw_argc;
    return raw_argc > 0;
}

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
    char path_copy[PATH_MAX];
    strcpy(path_copy,env_path);
    char *dir=strtok(path_copy,":");
    while(dir!=NULL){
        snprintf(result,PATH_MAX,"%s/%s",dir,filename);
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

int resolve_command(char *command, char *path, char **argv0){
    *argv0 = command;
    if (strchr(command, '/') != NULL) {
        if (!is_executable(command)) {
            return 0;
        }
        snprintf(path, PATH_MAX, "%s", command);
        return 1;
    }
    if (command[0] == '%') {
        *argv0 = command + 1;
        return find_path(*argv0, path);
    }
    snprintf(path, PATH_MAX, "./%s", command);
    if (is_executable(path)) {
        *argv0 = command;
        return 1;
    }
    *argv0 = command;
    return find_path(command, path);
}

void close_pipeline_pipes(int (*pipes)[2], int pipe_count){
    for (int i = 0; i < pipe_count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

int start_output_copier(struct pipeline_stage *stages,int stage_index,int (*pipes)[2],int pipe_count,int stage_count){
    struct pipeline_stage *stage = &stages[stage_index];
    if (pipe(stage->output_pipe) == -1) {
        return 0;
    }
    stage->output_writer = fork();
    if (stage->output_writer == -1) {
        close(stage->output_pipe[0]);
        close(stage->output_pipe[1]);
        stage->output_pipe[0] = -1;
        stage->output_pipe[1] = -1;
        return 0;
    }
    if (stage->output_writer == 0) {
        close(stage->output_pipe[1]);
        close_pipeline_pipes(pipes, pipe_count);
        for (int i = 0; i < stage_count; i++) {
            if (stages[i].input_fd != -1) {
                close(stages[i].input_fd);
            }
            if (i != stage_index && stages[i].output_count > 0 &&
                !stages[i].setup_failed) {
                for (int j = 0; j < stages[i].output_count; j++) {
                    close(stages[i].output_fds[j]);
                }
            }
        }

        char buffer[4096];
        ssize_t bytes;
        while ((bytes = read(stage->output_pipe[0], buffer,sizeof(buffer))) > 0) {
            for (int i = 0; i < stage->output_count; i++){
                ssize_t written = 0;
                while (written < bytes) {
                    ssize_t result = write(stage->output_fds[i],buffer + written,(size_t)(bytes - written));
                    if (result <= 0) {
                        _exit(1);
                    }
                    written += result;
                }
            }
        }
        close(stage->output_pipe[0]);
        for (int i = 0; i < stage->output_count; i++) {
            close(stage->output_fds[i]);
        }
        _exit(0);
    }
    return 1;
}

int has_pipeline(struct token *current){
    while (current != NULL && current->type != OP_SEMI &&current->type != OP_AMP){
        if (current->type == OP_PIPE) {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

int parse_pipeline(struct token *current,struct pipeline_stage **stages,int *stage_count){
    int count = 1;
    struct token *cursor = current;
    while (cursor != NULL && cursor->type != OP_SEMI &&
           cursor->type != OP_AMP) {
        if (cursor->type == OP_PIPE) {
            count++;
        }
        cursor = cursor->next;
    }
    struct pipeline_stage *result = calloc((size_t)count, sizeof(*result));
    if (result == NULL) {
        return 0;
    }
    struct token *start = current;
    cursor = current;
    int index = 0;
    while (cursor != NULL && cursor->type != OP_SEMI &&
           cursor->type != OP_AMP) {
        if (cursor->type == OP_PIPE) {
            if (!parse_pipeline_stage(start, cursor, &result[index++])) {
                free(result);
                return 0;
            }
            start = cursor->next;
        }
        cursor = cursor->next;
    }
    if (!parse_pipeline_stage(start, cursor, &result[index])) {
        free(result);
        return 0;
    }
    *stages = result;
    *stage_count = count;
    return 1;
}

void run_pipeline(struct pipeline_stage *stages, int stage_count){
    int pipe_count = stage_count - 1;
    int pipes[100][2];
    pid_t children[100];
    for (int i = 0; i < stage_count; i++) {
        stages[i].input_fd = -1;
        stages[i].input_writer = -1;
        if (stages[i].input_count > 0) {
            stages[i].input_fd = setup_input(stages[i].input_files,stages[i].input_count,&stages[i].input_writer);
            if (stages[i].input_fd == -1) {
                stages[i].setup_failed = 1;
            }
        }
        if (stages[i].output_count > 0 &&
            !open_output_files(stages[i].output_files,stages[i].output_count,stages[i].output_fds)){
            stages[i].setup_failed = 1;
        }
    }

    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipes[i]) == -1) {
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            for (int j = 0; j < stage_count; j++) {
                if (stages[j].input_fd != -1) {
                    close(stages[j].input_fd);
                }
                if (stages[j].input_writer != -1) {
                    waitpid(stages[j].input_writer, NULL, 0);
                }
                if (!stages[j].setup_failed) {
                    for (int k = 0; k < stages[j].output_count; k++) {
                        close(stages[j].output_fds[k]);
                    }
                }
            }
            return;
        }
    }

    for (int i = 0; i < stage_count; i++) {
        if (stages[i].output_count > 1 && !stages[i].setup_failed &&
            !start_output_copier(stages, i, pipes, pipe_count, stage_count)) {
            stages[i].setup_failed = 1;
            for (int j = 0; j < stages[i].output_count; j++) {
                close(stages[i].output_fds[j]);
            }
        }
    }

    for (int i = 0; i < stage_count; i++) {
        children[i] = fork();
        if (children[i] == -1) {
            children[i] = -1;
            continue;
        }
        if (children[i] == 0) {
            char path[PATH_MAX];
            char *argv0;

            if (stages[i].setup_failed) {
                if (stages[i].input_count > 0) {
                    fprintf(stderr, "cshell: no such file or directory\n");
                }
                _exit(1);
            }
            if (!resolve_command(stages[i].argv[0], path, &argv0)) {
                fprintf(stderr, "cshell: command not found (%s)\n",
                        argv0);
                _exit(1);
            }
            stages[i].argv[0] = argv0;

            if (stages[i].input_fd != -1) {
                if (dup2(stages[i].input_fd, STDIN_FILENO) == -1) {
                    _exit(1);
                }
            }
            else if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                    _exit(1);
                }
            }
            if (stages[i].output_count == 0 && i < stage_count - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    _exit(1);
                }
            }
            else if (stages[i].output_count == 1 &&
                     !stages[i].setup_failed) {
                if (dup2(stages[i].output_fds[0], STDOUT_FILENO) == -1) {
                    _exit(1);
                }
            }
            else if (stages[i].output_count > 1 &&
                     !stages[i].setup_failed) {
                if (dup2(stages[i].output_pipe[1], STDOUT_FILENO) == -1) {
                    _exit(1);
                }
            }
            for (int j = 0; j < pipe_count; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            for (int j = 0; j < stage_count; j++) {
                if (stages[j].output_count > 1 &&
                    !stages[j].setup_failed) {
                    if (j == i) {
                        close(stages[j].output_pipe[0]);
                    } else {
                        close(stages[j].output_pipe[1]);
                        close(stages[j].output_pipe[0]);
                    }
                }
            }
            if (stages[i].input_fd != -1) {
                close(stages[i].input_fd);
            }
            for (int j = 0; j < stage_count; j++) {
                if (j != i && stages[j].input_fd != -1) {
                    close(stages[j].input_fd);
                }
            }
            for (int j = 0; j < stage_count; j++) {
                if (stages[j].output_count > 0 && !stages[j].setup_failed) {
                    for (int k = 0; k < stages[j].output_count; k++) {
                        close(stages[j].output_fds[k]);
                    }
                }
            }
            execv(path, stages[i].argv);
            _exit(1);
        }
    }
    close_pipeline_pipes(pipes, pipe_count);
    for (int i = 0; i < stage_count; i++) {
        if (stages[i].output_pipe[0] != -1) {
            close(stages[i].output_pipe[0]);
            close(stages[i].output_pipe[1]);
        }
        if (stages[i].input_fd != -1) {
            close(stages[i].input_fd);
        }
        if (stages[i].input_writer != -1) {
            waitpid(stages[i].input_writer, NULL, 0);
        }
        if (!stages[i].setup_failed) {
            for (int j = 0; j < stages[i].output_count; j++) {
                close(stages[i].output_fds[j]);
            }
        }
    }
    for(int i = 0; i < stage_count; i++) {
        if (children[i] != -1) {
            waitpid(children[i], NULL, 0);
        }
        if (stages[i].output_writer != -1) {
            waitpid(stages[i].output_writer, NULL, 0);
        }
    }
}

void exec_command(struct token *current){
    if (has_pipeline(current)) {
        struct pipeline_stage *stages;
        int stage_count;
        if (!parse_pipeline(current, &stages, &stage_count)) {
            printf("cshell: invalid syntax\n");
            return;
        }
        run_pipeline(stages, stage_count);
        free(stages);
        return;
    }
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
    char path[PATH_MAX];

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
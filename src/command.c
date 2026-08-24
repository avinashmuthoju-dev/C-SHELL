#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>

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

void run_command(char *path,char **argv){
    pid_t pid=fork();

    if(pid==0){
        execv(path,argv);
        exit(1);
    }

    waitpid(pid,NULL,0);
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

void exec_command(struct token *current){

    char *argv[100];
    int argc;

    build_argv(current,argv,&argc);

    char *command=argv[0];
    char path[4096];

    if(strchr(command,'/')!=NULL){
      if(is_executable(command)){
        run_command(command,argv);
      }
      else{
        printf("cshell: command not found (%s)\n", command);
      }
    }
    else if(command[0]=='%'){

       char *name=command+1;
       if(find_path(name,path)){
          argv[0]=name;
          run_command(path,argv);
        }
        else{
          printf("cshell: command not found (%s)\n",name);
        }
    }
    else{
        snprintf(path,sizeof(path),"./%s",command);

        if(is_executable(path)){
           run_command(path,argv);
        }
        else if(find_path(command,path)){
          run_command(path,argv);
        }   
        else{
          printf("cshell: command not found (%s)\n",command);
        }
    }
}
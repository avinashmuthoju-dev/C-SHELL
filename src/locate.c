#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>

#include "locate.h"
#include "lexer.h"


int is_executable(char *path){
    struct stat st;
    if(stat(path,&st)!=0){
        return 0;
    }
    if(!S_ISREG(st.st_mode)){
        return 0;
    }
    if(access(path,X_OK)!=0){
        return 0;
    }
    return 1;
}

int search_current_directory(char *filename){

    char cwd[1024];
    char full_path[4096];

    if(getcwd(cwd,sizeof(cwd))==NULL){
        return 0;
    }

    snprintf(full_path,sizeof(full_path),"%s/%s",cwd,filename);

    if(is_executable(full_path)){
        printf("%s\n",full_path);
        return 1;
    }
    return 0;
}

int search_path(char *filename){
    int found=0;
    char *path=getenv("PATH");

    if(path==NULL){
        return 0;
    }
        
    char path_copy[4096];
    strcpy(path_copy,path);

    char *dir=strtok(path_copy,":");

    while(dir!=NULL){

        char full_path[2048];

        snprintf(full_path,sizeof(full_path),"%s/%s",dir,filename);

        if(is_executable(full_path)){
            printf("%s\n",full_path);
            found=1;
        }

        dir=strtok(NULL,":");
    }
    return found;
}

void exec_locate(struct token *current){

    current=current->next;

    if(current==NULL){
        printf("locate: invalid syntax\n");
        return;
    }

    while(current!=NULL){
        
        int found=0;
        
        if(search_current_directory(current->value)){
            found=1;
        }
        if(search_path(current->value)){
            found=1;
        }

        if(!found){
            printf("locate: command not found (%s)\n",current->value);
        }
        current=current->next;
    }
}
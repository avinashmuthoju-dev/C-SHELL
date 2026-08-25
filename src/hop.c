#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<limits.h>
#include "hop.h"
#include "lexer.h"
#include "frecency.h"

char home_Dir[PATH_MAX];
char prev_dir[PATH_MAX];
int prev_dir_flag=0;

void init_hop(char *home_dir_path){
    strcpy(home_Dir,home_dir_path);
    init_frecency();
}
int change_dir(char *path){
    char curr_wd[PATH_MAX];

    if(getcwd(curr_wd,sizeof(curr_wd))==NULL){
        return 0;
    }
    if(chdir(path)!=0){
        return 0;
    }
    strcpy(prev_dir,curr_wd);
    prev_dir_flag=1;

    char new_wd[PATH_MAX];
    if(getcwd(new_wd, sizeof(new_wd)) != NULL) {
        record_visit(new_wd);
    }

    return 1;
}
void exec_hop(struct token *current){

    current=current->next; //hop -> next argument;
    
    if(current==NULL){
        change_dir(home_Dir);
        return;
    }

    while(current!=NULL){
        if(strcmp(current->value,"~")==0){
            change_dir(home_Dir);
        }
        else if(strcmp(current->value,".")==0){

        }
        else if(strcmp(current->value,"..")==0){
            change_dir("..");
        }
        else if(strcmp(current->value,"-")==0){
            if(prev_dir_flag){
                change_dir(prev_dir);
                // char curr_direct[1024];
                // if(getcwd(curr_direct,sizeof(curr_direct))!=NULL){
                //     if(chdir(prev_dir)==0){
                //         strcpy(prev_dir,curr_direct);
                //     }
                // }
            }
        }
        else{
            if(!change_dir(current->value)){
                char result[PATH_MAX];
                if(find_frecency_match(current->value,result)){
                    if(!change_dir(result)){
                        printf("hop: no such directory\n");
                    }
                }
                else{
                    printf("hop: no such directory\n");
                }
            }
        }
        current=current->next;
    }
}
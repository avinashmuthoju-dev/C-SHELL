#include<stdio.h>
#include<stdlib.h>
#include<dirent.h>
#include<sys/stat.h>
#include<string.h>
#include <unistd.h>

#include "reveal.h"
#include "lexer.h"
#include "hop.h"

int compare(const void *a,const void *b){
    const char *name1=*(const char **)a;
    const char *name2=*(const char **)b;

    return strcmp(name1,name2);
}

char *resolve_path(char *path){
    if(strcmp(path, "~") == 0){
        return home_Dir;
    }

    if(strcmp(path, "-")== 0){
        if(prev_dir_flag){
            return prev_dir;
        }
        return NULL;
    }
    return path;
}

void reveal_directory(char *path,char *display_path,int show_all){

    DIR *dir=opendir(path);

    if(dir==NULL){
        return;
    }

    struct dirent *entry;
    char **entries=NULL;
    int count=0;

    while((entry=readdir(dir))!=NULL){
        if(!show_all && entry->d_name[0]=='.'){
            continue;
        }
        if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }

        char **temp=realloc(entries,(count+1)*sizeof(char *));

        if(temp==NULL){
            closedir(dir);

            for(int i=0;i<count;i++){
                free(entries[i]);
            }
            free(entries);
            return;
        }

        entries=temp;

        entries[count]=malloc(strlen(entry->d_name)+1);

        if(entries[count]==NULL){
            closedir(dir);

            for(int i=0;i<count;i++){
                free(entries[i]);
            }

            free(entries);
            return;
        }

        strcpy(entries[count],entry->d_name);
        count++;
    }

    closedir(dir);
    qsort(entries,count,sizeof(char *),compare);

    for(int i=0;i<count;i++){
        char full_path[4096];
        char current_display[4096];

        snprintf(full_path,sizeof(full_path), "%s/%s",path,entries[i]);

        if(display_path[0]=='\0'){
            snprintf(current_display,sizeof(current_display),"%s",entries[i]);
        }
        else{
            snprintf(current_display,sizeof(current_display),"%s/%s",display_path,entries[i]);           
        }

        struct stat st;
        if(stat(full_path,&st)!=0){
            free(entries[i]);
            continue;
        }

        if(S_ISDIR(st.st_mode)){

            printf("%s/\n",current_display);

            reveal_directory(full_path,current_display,show_all);
        }
        else{
            printf("%s\n",current_display);
        }
        free(entries[i]);
    }
    free(entries);
}

void exec_reveal(struct token *current){
    current=current->next;

    int show_all=0;
    int rec=0;
    int path_count=0;
    char *path=NULL;

    while(current!=NULL){
        if(current->value[0]=='-'){
            for(int i=1;current->value[i]!='\0';i++){
              if(current->value[i]=='a'){
                show_all=1;
              }
              else if(current->value[i]=='t'){
                rec=1;
              }
              else{
                printf("reveal: invalid syntax\n");
                return;
              }
           }
        }
        else{
            path_count++;
            if(path_count>1){
                printf("reveal: invalid syntax\n");
                return;
            }
            path=current->value;
        }
        current=current->next;
    }

    if(path==NULL){
        path=".";
    }
    path=resolve_path(path);
    if(path == NULL){
        printf("reveal: no such directory\n");
        return;
    }

    DIR *temp=opendir(path);
    if(temp==NULL){
        printf("reveal: no such directory\n");
        return;        
    }

    closedir(temp);

    
    if(rec){
       if(strcmp(path,".")==0){
        reveal_directory(path,"",show_all);
       }
       else{
        reveal_directory(path,path,show_all);
       }
    }
    else{
        DIR *dir=opendir(path);
        if(dir==NULL){
           printf("reveal: no such directory\n");
           return;
        }
        char **entries=NULL;
        int count=0;

        struct dirent *entry;
        while((entry=readdir(dir))!=NULL){
           if(!show_all && entry->d_name[0]=='.') continue;

           char **temp = realloc(entries, (count + 1) * sizeof(char *));
           if(temp == NULL){
            for(int i=0;i<count;i++){
                free(entries[i]);
            }
            free(entries);
            closedir(dir);
            return;
           }

           entries=temp;
           entries[count]=malloc(strlen(entry->d_name) + 1);
           if(entries[count]==NULL){
            for(int i=0;i<count;i++){
                free(entries[i]);
            }
            free(entries);
            closedir(dir);
            return;
           }
            strcpy(entries[count], entry->d_name);
            count++;
        }

        closedir(dir);
        qsort(entries, count, sizeof(char *), compare);
        for(int i=0;i<count;i++){
            printf("%s\n",entries[i]);
            free(entries[i]);
        }
        free(entries);
    }
}
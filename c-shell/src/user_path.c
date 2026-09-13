#include<string.h>
#include "user_path.h"

void get_path(char* path,char *home_dir,char *curr_dir){
    int home_len=strlen(home_dir);
    if(strcmp(home_dir,curr_dir)==0){
        strcpy(path,"~");
    }
    else if(strncmp(home_dir,curr_dir,home_len)==0 && curr_dir[home_len]=='/'){
        strcpy(path,"~");
        strcat(path,curr_dir+home_len);
    }
    else{
        strcpy(path,curr_dir);
    }
}
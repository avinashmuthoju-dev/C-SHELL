#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>

char curr_dir[1024];
char host_name[1024];
char home_dir[1024];

void get_path(char* path){
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

int main(){
    
    char *user_name=getenv("USER");
    gethostname(host_name, sizeof(host_name));
    getcwd(home_dir,sizeof(home_dir));
    char path[1024];
    char input[1024];
    while(1){
        
        getcwd(curr_dir,sizeof(curr_dir));

        get_path(path);
        printf("<%s@%s:%s>",user_name,host_name,path);
        scanf("%s",input);
    }    
    
}
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include "user_path.h"
#include "lexer.h"
#include "parser.h"
#include "hop.h"

char curr_dir[1024];
char host_name[1024];
char home_dir[1024];

int main(){
    
    char *user_name=getenv("USER");
    gethostname(host_name, sizeof(host_name));
    getcwd(home_dir,sizeof(home_dir));
    init_hop(home_dir);
    char path[1024];
    char input[1024];
    while(1){
        
        getcwd(curr_dir,sizeof(curr_dir));

        get_path(path,home_dir,curr_dir);
        printf("<%s@%s:%s>",user_name,host_name,path);
        
        fgets(input,sizeof(input),stdin);
        input[strcspn(input,"\n")]='\0';
        free_tokens();
        if(lexing(input)==0){
            continue;
        }
        if(!parsing()){
            printf("cshell: invalid syntax\n");
        }
        else{
            if(strcmp(Token->value,"hop")==0){
                exec_hop(Token);
            }
        }

    }    
    
}
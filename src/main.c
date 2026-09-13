#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<limits.h>

#include "user_path.h"
#include "lexer.h"
#include "parser.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include "command.h"
#include "sequence.h"
#include "background.h"
#include "activities.h"
#include "terminal.h"
#include "resume.h"

char curr_dir[PATH_MAX];
char host_name[1024];
char home_dir[PATH_MAX];

int main(){

    init_background();
    consume_signal_newline();
    init_job_control();
    char *user_name=getenv("USER");
    gethostname(host_name, sizeof(host_name));
    getcwd(home_dir,sizeof(home_dir));
    init_hop(home_dir);
    char path[PATH_MAX];
    char input[1024];
    while(1){
        
        getcwd(curr_dir,sizeof(curr_dir));

        get_path(path,home_dir,curr_dir);
        consume_signal_newline();
        printf("<%s@%s:%s>",user_name,host_name,path);
        
        if(fgets(input,sizeof(input),stdin)==NULL){
           consume_signal_newline();
           if(handle_eof()){
            break;
           }
           clearerr(stdin);
           continue;
        }
        reset_eof_state();
        input[strcspn(input,"\n")]='\0';
        free_tokens();
        if(lexing(input)==0){
            continue;
        }
        if(Token==NULL){
            continue;
        }
        if(!parsing()){
            printf("cshell: invalid syntax\n");
        }
        else{
            if(strcmp(Token->value,"hop")==0){
                exec_hop(Token);
            }
            else if(strcmp(Token->value,"reveal")==0){
                exec_reveal(Token);
            }
            else if(strcmp(Token->value,"peek")==0){
                exec_peak(Token);
            }
            else if(strcmp(Token->value,"locate")==0){
                exec_locate(Token);
            }
            else if(strcmp(Token->value,"activities")==0){
                exec_activities();
            }
            else if(strcmp(Token->value,"resume")==0){
                exec_resume(Token);
            }
            else{
                sequence_command(Token);
            }
        }

        drain_background_notifications();
    }    

}
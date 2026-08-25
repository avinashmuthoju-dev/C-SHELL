#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>
#include<fcntl.h>
#include<sys/stat.h>

#include "peek.h"
#include "lexer.h"

int is_empty(char *line){
    for(int i=0;line[i]!='\0';i++){
        if(!isspace(line[i])){
            return 0;
        }
    }
    return 1;
}

int get_line_count(FILE *fp){
    int count=0;
    char line[1024];
    while(fgets(line,sizeof(line),fp)!=NULL){
        if(!is_empty(line)){
            count++;
        }
    }
    return count;
}

void nums_line(FILE *fp,int *count){
    char line[1024];
    while(fgets(line,sizeof(line),fp)!=NULL){
        if(!is_empty(line)){
            (*count)++;
            printf("%d %s",*count,line);
        }
        else{
            printf("%s",line);
        }
    }
}

void reverse(FILE *fp){
    int fd=fileno(fp);
    off_t end=lseek(fd,0,SEEK_END);
    if(end==-1){
        return;
    }
    char buffer[1024];
    char line[1024];

    int line_pos=0;
    off_t pos=end;

    while(pos>0){

        off_t start=pos-sizeof(buffer);

        if(start<0){
            start=0;
        }

        lseek(fd,start,SEEK_SET);

        ssize_t bytes=read(fd,buffer,pos-start);

        if(bytes<=0){
            return;
        }

        for(ssize_t i=bytes-1;i>=0;i--){

            if(buffer[i]=='\n'){
                for(int j=line_pos-1;j>=0;j--){
                    putchar(line[j]);
                }
                putchar('\n');
                line_pos=0;
            }
            else{
                line[line_pos++]=buffer[i];
            }
        }

        pos=start;
    }
    if(line_pos>0){
        for(int j=line_pos-1;j>=0;j--){
            putchar(line[j]);
        }
        putchar('\n');
    }
}

void reverse_with_nums(FILE *fp,int total_lines){
    int fd=fileno(fp);
    off_t pos=lseek(fd,0,SEEK_END);
    if(pos==-1){
        return;
    }

    char buffer[1024];
    char line[1024];
    int line_pos=0;
    int line_number=total_lines;

    while(pos>0){
        off_t start=pos-sizeof(buffer);

        if(start<0){
            start=0;
        }
        if(lseek(fd,start,SEEK_SET)==-1){
            return;
        }
        ssize_t bytes=read(fd,buffer,pos-start);
        if(bytes<=0){
            return;
        }

        for(ssize_t i=bytes-1;i>=0;i--){
            if(buffer[i]=='\n'){
                int empty=1;
                for(int j=0;j<line_pos;j++){
                    if(!isspace(line[j])){
                        empty=0;
                        break;
                    }
                }
                if(!empty){
                    printf("%d ",line_number);
                    line_number--;
                }
                for(int j=line_pos-1;j>=0;j--){
                    putchar(line[j]);
                }

                putchar('\n');

                line_pos=0;
            }
            else{
                if(line_pos<(int)sizeof(line)- 1){
                    line[line_pos++]=buffer[i];
                }
            }
        }

        pos=start;
    }

    if(line_pos>0){

        int empty=1;
        for(int j=0;j<line_pos;j++){
            if(!isspace(line[j])){
                empty=0;
                break;
            }
        }
        if(!empty){
            printf("%d ",line_number);
            line_number--;
        }

        for(int j=line_pos-1;j>=0;j--){
            putchar(line[j]);
        }
        putchar('\n');
    }
}

void exec_peak(struct token *current){

    current=current->next;
    int show_nums=0;
    int rever=0;

    char *files[100];
    int file_count=0;

    while(current!=NULL){

        if(strcmp(current->value,"-")==0){
            int fd=STDIN_FILENO;
            char buffer[1024];
            ssize_t bytes;

            while((bytes=read(fd,buffer,sizeof(buffer)))>0){
                write(STDOUT_FILENO,buffer,bytes);
            }
            current=current->next;
            continue;
        }
        if(current->value[0]=='-'){
            for(int i=1;current->value[i]!='\0';i++){
              if(current->value[i]=='n'){
                show_nums=1;
              }
              else if(current->value[i]=='r'){
                rever=1;
              }
              else{
                printf("peak: invalid syntax");
                return ;
              }               
            }
        }
        else{
            files[file_count++]=current->value;
        }    
        current=current->next;
    }

    if(file_count==0){
        int fd=STDIN_FILENO;
        char buffer[1024];
        ssize_t bytes;

        while((bytes=read(fd,buffer,sizeof(buffer)))>0){
           write(STDOUT_FILENO,buffer,bytes);
        }
        return;
    }
    int line_count=0;
    for(int i=0;i<file_count;i++){
        
        struct stat st;
        if(stat(files[i],&st)!=0){
            printf("peek: no such file or directory\n");
            return;
        }
        if(S_ISDIR(st.st_mode)){
            printf("peek: is a directory\n");
            return;
        }

        FILE *fp=fopen(files[i],"r");

        if(fp==NULL){
            printf("peek: no such file or directory\n");
            return;
        }
        if(show_nums && rever){
            int total_lines=get_line_count(fp);
            rewind(fp);
            reverse_with_nums(fp,total_lines);
            fclose(fp);
            continue;
        }
        else if(show_nums){
          nums_line(fp,&line_count);
          fclose(fp);
          continue;
        }
        else if(rever){
            reverse(fp);
            fclose(fp);
            continue;
        }
        else{
          int ch;
          while((ch=fgetc(fp))!=EOF){
            putchar(ch);
          }
        }
        fclose(fp);
    }
}
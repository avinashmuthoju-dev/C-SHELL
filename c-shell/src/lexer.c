#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "lexer.h"

struct token *Token = NULL;
void free_tokens(){
    struct token *temp;
    while(Token!=NULL){
        temp=Token;
        Token=Token->next;
        free(temp->value);
        free(temp);
    }
}
void add_token(enum Token_type type,char *value){
    struct token *new=malloc(sizeof(struct token));
    new->type=type;
    new->value=malloc(strlen(value)+1);
    strcpy(new->value,value);
    new->next=NULL;

    if(Token==NULL){
        Token=new;
        return;
    }
    struct token *temp=Token;
    while(temp->next!=NULL){
        temp=temp->next;
    }
    temp->next=new;
}
int is_space(char c){
    return c==' ' || c=='\t' || c=='\n' || c=='\r';
}
int is_special(char c){
    return c=='|' || c=='&' || c=='>' || c=='<' || c==';';
}
int backslash_quote(char *line,int *pos,char *word,int *word_pos){
    (*pos)++;
    if(line[*pos]=='\0'){
        return 0;
    }
    word[(*word_pos)]=line[(*pos)];
    (*word_pos)++;
    (*pos)++;
    return 1;
}
int double_quote(char *line,int *pos,char *word,int *word_pos){
    (*pos)++;
    while(line[*pos]!='\0'){
        if(line[*pos]=='"'){
            (*pos)++;
            return 1;
        }
        if(line[*pos]=='\\'){      
            if(line[*pos+1]=='\0'){   // \ is the end
                return 0;
            }
            if(line[*pos+1]=='"'){      // \" becomes = "
                word[(*word_pos)++]='"';
                (*pos)+=2;
            }
            else if(line[*pos+1]=='\\'){   // \\ become \ ;
                word[(*word_pos)++]='\\';
                (*pos)+=2;
            }
            else{
                word[(*word_pos)++]='\\';  // \c becomes \ ;
                word[(*word_pos)++]=line[*pos+1];
                (*pos)+=2;
            }
        }
        else{
            word[*word_pos]=line[*pos];
            (*word_pos)++;
            (*pos)++;
        }
    }
    return 0;
}
int single_quote(char *line,int *pos,char *word,int *word_pos){
    (*pos)++;
    while(line[*pos]!='\0'){
        if(line[*pos]=='\''){
            (*pos)++;
            return 1;
        }
        word[*word_pos]=line[*pos];
        (*word_pos)++;
        (*pos)++;
    }
    return 0;
}
int lexing(char *line){
    int pos=0;
    while(line[pos]!='\0'){
        if(line[pos]==' ' || line[pos]=='\t' || line[pos]=='\n' || line[pos]=='\r'){
            pos++;
        }
        else if(line[pos]=='|'){
            add_token(OP_PIPE,"|");
            pos++;
        }
        else if(line[pos]=='&'){
            add_token(OP_AMP,"&");
            pos++;
        }
        else if(line[pos]==';'){
            add_token(OP_SEMI,";");
            pos++;
        }
        else if(line[pos]=='<'){
            add_token(OP_LT,"<");
            pos++;
        }
        else if(line[pos]=='>'){
            if(line[pos+1]!='\0' && line[pos+1]=='>'){
                add_token(OP_GTGT,">>");
                pos+=2;
            }
            else{
                add_token(OP_GT,">");
                pos++;
            }
        }
        else{
            char word[1024];
            int word_pos=0;
            while(line[pos]!='\0' && !is_space(line[pos]) && !is_special(line[pos])){
                if(line[pos]=='\\'){
                    if(!backslash_quote(line,&pos,word,&word_pos)){
                        printf("cshell: invalid syntax\n");
                        return 0;
                    }
                }
                else if(line[pos]=='"'){
                    if(!double_quote(line,&pos,word,&word_pos)){
                        printf("cshell: invalid syntax\n");
                        return 0;
                    }
                }
                else if(line[pos]=='\''){
                    if(!single_quote(line,&pos,word,&word_pos)){
                        printf("cshell: invalid syntax\n");
                        return 0;
                    }
                }
                else{
                    word[word_pos++]=line[pos];
                    pos++;
                }
            }
            word[word_pos]='\0';
            add_token(WORD,word);
        }
    }
    return 1;
}

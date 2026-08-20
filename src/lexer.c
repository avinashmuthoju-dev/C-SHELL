#include<stdio.h>
#include<stdlib.h>

enum Token_type{
    OP_PIPE,
    OP_AMP,
    OP_SEMI,
    OP_LT,
    OP_GT,
    OP_GTGT,
    WORD,
};
struct token{
    enum Token_type type;
    char *value;
    struct token *next;
};

struct token *Token;

void add_token(enum Token_type type,char *value){
    struct token *new=malloc(sizeof(struct token));
    new->type=type;
    new->value=value;
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
void lexing(char *line){
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

        }
    }
}

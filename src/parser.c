#include<stdio.h>
#include "lexer.h"
#include "parser.h"

struct token *current;

int parse_line(){
    if(current==NULL){
        return 1;
    }
    if(current->type==WORD){
       current=current->next;
       return parse_arg();
    }
    return 0;
}
int parse_arg(){
    if(current==NULL){
        return 1;
    }
    if(current->type==WORD){
        current=current->next;
        return parse_arg();
    }
    if(current->type==OP_LT){
        current=current->next;
        return parse_tgt();
    }
    if(current->type==OP_GT){
        current=current->next;
        return parse_tgt();
    }
    if(current->type==OP_GTGT){
        current=current->next;
        return parse_tgt();
    }
    if(current->type==OP_PIPE){
        current=current->next;
        return parse_cmd();
    }
    if(current->type==OP_SEMI){
        current=current->next;
        return parse_cmd();
    }
    if(current->type==OP_AMP){
        current=current->next;
        return parse_bg();
    }
    return 0;
}
int parse_cmd(){
    if(current==NULL){
        return 0;
    }
    if(current->type==WORD){
        current=current->next;
        return parse_arg();
    }
    return 0;
}
int parse_tgt(){
    if(current==NULL){
        return 0;
    }
    if(current->type==WORD){
        current=current->next;
        return parse_arg();
    }
    return 0;
}
int parse_bg(){
    if(current==NULL){
        return 1;
    }
    if(current->type==WORD){
        current=current->next;
        return parse_arg();
    }
    return 0;
}
int parsing(){
    current=Token;
    if(parse_line() && current == NULL){
        return 1;
    }
    return 0;
}
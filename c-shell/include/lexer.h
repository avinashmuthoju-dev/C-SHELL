#ifndef LEXER_H
#define LEXER_H

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

extern struct token *Token;

void free_tokens();
void add_token(enum Token_type type,char *value);
int is_space(char c);
int is_special(char c);
int double_quote(char *line,int *pos,char *word,int *word_pos);
int single_quote(char *line,int *pos,char *word,int *word_pos);
int backslash_quote(char *line,int *pos,char *word,int *word_pos);
int lexing(char *line);

#endif
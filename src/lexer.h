#ifndef MJ_LEXER_H
#define MJ_LEXER_H

#include "common.h"

typedef enum {
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_QUESTION,

    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,

    TOKEN_PLUS,
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH,
    TOKEN_SLASH_EQUAL,
    TOKEN_PERCENT,
    TOKEN_PERCENT_EQUAL,

    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,

    TOKEN_AND,
    TOKEN_OR,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_FN,
    TOKEN_RETURN,
    TOKEN_LET,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NIL,
    TOKEN_STRUCT,
    TOKEN_IMPORT,
    TOKEN_AS,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_ASSERT,
    TOKEN_TYPEOF,
    TOKEN_LENGTH,

    TOKEN_ERROR,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

typedef struct {
    const char* source;
    const char* start;
    const char* current;
    int line;
} Lexer;

void lexer_init(Lexer* lexer, const char* source);
Token lexer_scan(Lexer* lexer);
const char* token_type_name(TokenType type);

#endif

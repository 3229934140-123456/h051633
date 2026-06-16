#include <string.h>
#include <stdio.h>

#include "lexer.h"
#include "common.h"

void lexer_init(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static bool is_at_end(Lexer* lexer) {
    return *lexer->current == '\0';
}

static char advance(Lexer* lexer) {
    lexer->current++;
    return lexer->current[-1];
}

static char peek(Lexer* lexer) {
    return *lexer->current;
}

static char peek_next(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static bool match(Lexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    lexer->current++;
    return true;
}

static Token make_token(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    return token;
}

static Token error_token(Lexer* lexer, const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    return token;
}

static void skip_whitespace(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                advance(lexer);
                break;
            case '/':
                if (peek_next(lexer) == '/') {
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                        advance(lexer);
                    }
                } else if (peek_next(lexer) == '*') {
                    advance(lexer);
                    advance(lexer);
                    while (!(peek(lexer) == '*' && peek_next(lexer) == '/') && !is_at_end(lexer)) {
                        if (peek(lexer) == '\n') lexer->line++;
                        advance(lexer);
                    }
                    if (!is_at_end(lexer)) {
                        advance(lexer);
                        advance(lexer);
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType check_keyword(Lexer* lexer, int start, int length,
                               const char* rest, TokenType type) {
    if (lexer->current - lexer->start == start + length &&
        memcmp(lexer->start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType identifier_type(Lexer* lexer) {
    int length = (int)(lexer->current - lexer->start);
    const char* start = lexer->start;

    switch (start[0]) {
        case 'a':
            if (length >= 2) {
                switch (start[1]) {
                    case 'n': return check_keyword(lexer, 2, 1, "d", TOKEN_AND);
                    case 's':
                        if (length == 2) return TOKEN_AS;
                        if (length == 6 && memcmp(start + 2, "sert", 4) == 0) return TOKEN_ASSERT;
                        break;
                }
            }
            break;
        case 'b': return check_keyword(lexer, 1, 4, "reak", TOKEN_BREAK);
        case 'c': return check_keyword(lexer, 1, 7, "ontinue", TOKEN_CONTINUE);
        case 'e': return check_keyword(lexer, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (length >= 2) {
                switch (start[1]) {
                    case 'n':
                        if (length == 2) return TOKEN_FN;
                        break;
                    case 'a': return check_keyword(lexer, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(lexer, 2, 1, "r", TOKEN_FOR);
                }
            }
            break;
        case 'i':
            if (length >= 2) {
                switch (start[1]) {
                    case 'f':
                        if (length == 2) return TOKEN_IF;
                        break;
                    case 'm':
                    case 'n':
                        return check_keyword(lexer, 1, 6, "mport", TOKEN_IMPORT);
                }
            }
            break;
        case 'l':
            if (length >= 2 && start[1] == 'e') {
                if (length == 3) return TOKEN_LET;
                if (length == 6 && memcmp(start + 2, "ngth", 4) == 0) return TOKEN_LENGTH;
            }
            break;
        case 'n': return check_keyword(lexer, 1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(lexer, 1, 1, "r", TOKEN_OR);
        case 'r': return check_keyword(lexer, 1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(lexer, 1, 5, "truct", TOKEN_STRUCT);
        case 't':
            if (length >= 2) {
                switch (start[1]) {
                    case 'r': return check_keyword(lexer, 2, 2, "ue", TOKEN_TRUE);
                    case 'y': return check_keyword(lexer, 2, 4, "peof", TOKEN_TYPEOF);
                }
            }
            break;
        case 'w': return check_keyword(lexer, 1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

static Token scan_identifier(Lexer* lexer) {
    while (is_alpha(peek(lexer)) || is_digit(peek(lexer))) {
        advance(lexer);
    }
    return make_token(lexer, identifier_type(lexer));
}

static Token scan_number(Lexer* lexer) {
    while (is_digit(peek(lexer))) advance(lexer);

    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        advance(lexer);
        while (is_digit(peek(lexer))) advance(lexer);
    }

    return make_token(lexer, TOKEN_NUMBER);
}

static Token scan_string(Lexer* lexer) {
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\\' && peek_next(lexer) != '\0') {
            advance(lexer);
        }
        if (peek(lexer) == '\n') lexer->line++;
        advance(lexer);
    }

    if (is_at_end(lexer)) return error_token(lexer, "Unterminated string.");

    advance(lexer);
    return make_token(lexer, TOKEN_STRING);
}

Token lexer_scan(Lexer* lexer) {
    skip_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer)) return make_token(lexer, TOKEN_EOF);

    char c = advance(lexer);

    if (is_alpha(c)) return scan_identifier(lexer);
    if (is_digit(c)) return scan_number(lexer);

    switch (c) {
        case '(': return make_token(lexer, TOKEN_LEFT_PAREN);
        case ')': return make_token(lexer, TOKEN_RIGHT_PAREN);
        case '[': return make_token(lexer, TOKEN_LEFT_BRACKET);
        case ']': return make_token(lexer, TOKEN_RIGHT_BRACKET);
        case '{': return make_token(lexer, TOKEN_LEFT_BRACE);
        case '}': return make_token(lexer, TOKEN_RIGHT_BRACE);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case '.': return make_token(lexer, TOKEN_DOT);
        case ';': return make_token(lexer, TOKEN_SEMICOLON);
        case ':': return make_token(lexer, TOKEN_COLON);
        case '?': return make_token(lexer, TOKEN_QUESTION);

        case '!':
            return make_token(lexer, match(lexer, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return make_token(lexer, match(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '>':
            return make_token(lexer, match(lexer, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '<':
            return make_token(lexer, match(lexer, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);

        case '+':
            return make_token(lexer, match(lexer, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
        case '-':
            return make_token(lexer, match(lexer, '=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
        case '*':
            return make_token(lexer, match(lexer, '=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
        case '/':
            return make_token(lexer, match(lexer, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
        case '%':
            return make_token(lexer, match(lexer, '=') ? TOKEN_PERCENT_EQUAL : TOKEN_PERCENT);

        case '"': return scan_string(lexer);
    }

    return error_token(lexer, "Unexpected character.");
}

const char* token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TOKEN_LEFT_BRACKET: return "LEFT_BRACKET";
        case TOKEN_RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TOKEN_LEFT_BRACE: return "LEFT_BRACE";
        case TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_DOT: return "DOT";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COLON: return "COLON";
        case TOKEN_QUESTION: return "QUESTION";
        case TOKEN_BANG: return "BANG";
        case TOKEN_BANG_EQUAL: return "BANG_EQUAL";
        case TOKEN_EQUAL: return "EQUAL";
        case TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TOKEN_GREATER: return "GREATER";
        case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_LESS: return "LESS";
        case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_PLUS_EQUAL: return "PLUS_EQUAL";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_MINUS_EQUAL: return "MINUS_EQUAL";
        case TOKEN_STAR: return "STAR";
        case TOKEN_STAR_EQUAL: return "STAR_EQUAL";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_SLASH_EQUAL: return "SLASH_EQUAL";
        case TOKEN_PERCENT: return "PERCENT";
        case TOKEN_PERCENT_EQUAL: return "PERCENT_EQUAL";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_FN: return "FN";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_LET: return "LET";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_NIL: return "NIL";
        case TOKEN_STRUCT: return "STRUCT";
        case TOKEN_IMPORT: return "IMPORT";
        case TOKEN_AS: return "AS";
        case TOKEN_BREAK: return "BREAK";
        case TOKEN_CONTINUE: return "CONTINUE";
        case TOKEN_ASSERT: return "ASSERT";
        case TOKEN_TYPEOF: return "TYPEOF";
        case TOKEN_LENGTH: return "LENGTH";
        case TOKEN_ERROR: return "ERROR";
        case TOKEN_EOF: return "EOF";
        default: return "UNKNOWN";
    }
}

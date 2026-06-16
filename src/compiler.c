#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "lexer.h"
#include "vm.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
    Lexer* lexer;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool is_captured;
} Local;

typedef struct {
    u8 index;
    bool is_local;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_MAX + 1];
    int local_count;
    Upvalue upvalues[UINT8_MAX + 1];
    int scope_depth;
} Compiler;

static Parser parser;
static Compiler* current = NULL;

static Chunk* current_chunk() {
    return &current->function->chunk;
}

static void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    parser.had_error = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
}

static void error_at_current(const char* message) {
    error_at(&parser.current, message);
}

static void error(const char* message) {
    error_at(&parser.previous, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = lexer_scan(parser.lexer);
        if (parser.current.type != TOKEN_ERROR) break;
        error_at_current(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match_token(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emit_byte(u8 byte) {
    chunk_write(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(u8 byte1, u8 byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_loop(int loop_start) {
    emit_byte(OP_LOOP);

    int offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static int emit_jump(u8 instruction) {
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->count - 2;
}

static void emit_return() {
    emit_byte(OP_NIL);
    emit_byte(OP_RETURN);
}

static u8 make_constant(Value value) {
    int constant = chunk_add_constant(current_chunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (u8)constant;
}

static void emit_constant(Value value) {
    emit_bytes(OP_CONSTANT, make_constant(value));
}

static void patch_jump(int offset) {
    int jump = current_chunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

static void init_compiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = new_function();
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copy_string(parser.previous.start,
                                              parser.previous.length);
    }

    Local* local = &current->locals[current->local_count++];
    local->depth = 0;
    local->is_captured = false;
    local->name.start = "";
    local->name.length = 0;
}

static ObjFunction* end_compiler() {
    emit_return();

    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        chunk_disassemble(&function->chunk,
                          function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

static void begin_scope() {
    current->scope_depth++;
}

static void end_scope() {
    current->scope_depth--;

    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth > current->scope_depth) {
        if (current->locals[current->local_count - 1].is_captured) {
            emit_byte(OP_CLOSE_UPVALUE);
        } else {
            emit_byte(OP_POP);
        }
        current->local_count--;
    }
}

static void declaration();
static void statement();
static void expression();
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);

static u8 identifier_constant(Token* name) {
    return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

static bool identifiers_equal(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(Compiler* compiler, Token* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error("Cannot read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static int add_upvalue(Compiler* compiler, u8 index, bool is_local) {
    int upvalue_count = compiler->function->upvalue_count;

    for (int i = 0; i < upvalue_count; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (upvalue_count == UINT8_MAX) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index = index;
    return compiler->function->upvalue_count++;
}

static int resolve_upvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (u8)local, true);
    }

    int upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (u8)upvalue, false);
    }

    return -1;
}

static void add_local(Token name) {
    if (current->local_count == UINT8_MAX) {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
}

static void declare_variable() {
    if (current->scope_depth == 0) return;

    Token* name = &parser.previous;

    for (int i = current->local_count - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth) break;

        if (identifiers_equal(name, &local->name)) {
            error("Variable with this name already declared in this scope.");
        }
    }

    add_local(*name);
}

static u8 parse_variable(const char* error_message) {
    consume(TOKEN_IDENTIFIER, error_message);

    declare_variable();
    if (current->scope_depth > 0) return 0;

    return identifier_constant(&parser.previous);
}

static void mark_initialized() {
    if (current->scope_depth == 0) return;
    current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(u8 global) {
    if (current->scope_depth > 0) {
        mark_initialized();
        return;
    }

    emit_bytes(OP_DEFINE_GLOBAL, global);
}

static u8 argument_list() {
    u8 arg_count = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (arg_count == 255) {
                error("Cannot have more than 255 arguments.");
            }
            arg_count++;
        } while (match_token(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return arg_count;
}

static void binary(bool can_assign) {
    TokenType operator_type = parser.previous.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precedence((Precedence)(rule->precedence + 1));

    switch (operator_type) {
        case TOKEN_BANG_EQUAL:    emit_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER:       emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emit_bytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          emit_byte(OP_ADD); break;
        case TOKEN_MINUS:         emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:          emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         emit_byte(OP_DIVIDE); break;
        case TOKEN_PERCENT:       emit_byte(OP_MOD); break;
        default: return;
    }
}

static void call(bool can_assign) {
    u8 arg_count = argument_list();
    emit_bytes(OP_CALL, arg_count);
}

static void dot(bool can_assign) {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    u8 name = identifier_constant(&parser.previous);

    if (can_assign && match_token(TOKEN_EQUAL)) {
        expression();
        emit_bytes(OP_SET_PROPERTY, name);
    } else {
        emit_bytes(OP_GET_PROPERTY, name);
    }
}

static void literal(bool can_assign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_NIL:   emit_byte(OP_NIL); break;
        case TOKEN_TRUE:  emit_byte(OP_TRUE); break;
        default: return;
    }
}

static void grouping(bool can_assign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool can_assign) {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string(bool can_assign) {
    const char* s = parser.previous.start + 1;
    int len = parser.previous.length - 2;
    char* unescaped = ALLOCATE(char, len + 1);
    int out_len = 0;
    for (int i = 0; i < len; i++) {
        if (s[i] == '\\' && i + 1 < len) {
            i++;
            switch (s[i]) {
                case 'n': unescaped[out_len++] = '\n'; break;
                case 't': unescaped[out_len++] = '\t'; break;
                case 'r': unescaped[out_len++] = '\r'; break;
                case '\\': unescaped[out_len++] = '\\'; break;
                case '"': unescaped[out_len++] = '"'; break;
                case '0': unescaped[out_len++] = '\0'; break;
                default: unescaped[out_len++] = s[i]; break;
            }
        } else {
            unescaped[out_len++] = s[i];
        }
    }
    unescaped[out_len] = '\0';
    emit_constant(OBJ_VAL(take_string(unescaped, out_len)));
}

static void named_variable(Token name, bool can_assign) {
    u8 get_op, set_op;
    int arg = resolve_local(current, &name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else if ((arg = resolve_upvalue(current, &name)) != -1) {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    } else {
        arg = identifier_constant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && match_token(TOKEN_EQUAL)) {
        expression();
        emit_bytes(set_op, (u8)arg);
    } else {
        emit_bytes(get_op, (u8)arg);
    }
}

static void variable(bool can_assign) {
    named_variable(parser.previous, can_assign);
}

static void unary(bool can_assign) {
    TokenType operator_type = parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (operator_type) {
        case TOKEN_BANG:  emit_byte(OP_NOT); break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return;
    }
}

static void and_(bool can_assign) {
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}

static void or_(bool can_assign) {
    int end_jump = emit_jump(OP_JUMP_IF_TRUE);

    emit_byte(OP_POP);
    parse_precedence(PREC_OR);

    patch_jump(end_jump);
}

static void typeof_(bool can_assign) {
    parse_precedence(PREC_UNARY);
    emit_byte(OP_TYPEOF);
}

static void length_expr(bool can_assign) {
    parse_precedence(PREC_CALL);
    emit_byte(OP_LENGTH);
}

static void subscript(bool can_assign) {
    expression();
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");

    if (can_assign && match_token(TOKEN_EQUAL)) {
        expression();
        emit_byte(OP_SET_INDEX);
    } else {
        emit_byte(OP_GET_INDEX);
    }
}

static void new_table_expr(bool can_assign) {
    emit_byte(OP_NEW_TABLE);

    if (check(TOKEN_RIGHT_BRACE)) {
        advance();
        return;
    }

    int count = 0;
    do {
        if (check(TOKEN_IDENTIFIER)) {
            Token key_token = parser.current;
            advance();
            consume(TOKEN_COLON, "Expect ':' after table key.");
            emit_byte(OP_DUP);
            expression();
            u8 name_idx = identifier_constant(&key_token);
            emit_bytes(OP_SET_PROPERTY, name_idx);
            emit_byte(OP_POP);
            count++;
        } else {
            error("Table literal requires identifier keys.");
            expression();
            emit_byte(OP_POP);
        }

        if (count > 255) {
            error("Table literal has too many entries.");
        }
    } while (match_token(TOKEN_COMMA));

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after table literal.");
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {new_table_expr, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACKET]  = {NULL,     subscript, PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     dot,    PREC_CALL},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_PERCENT]       = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FN]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_LET]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LENGTH]        = {length_expr, NULL, PREC_CALL},
    [TOKEN_TYPEOF]        = {typeof_,  NULL,   PREC_UNARY},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static ParseRule* get_rule(TokenType type) {
    return &rules[type];
}

static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && match_token(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
        expression();
    }
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
    Compiler compiler;
    init_compiler(&compiler, type);
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                error_at_current("Cannot have more than 255 parameters.");
            }
            u8 constant = parse_variable("Expect parameter name.");
            define_variable(constant);
        } while (match_token(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = end_compiler();
    emit_bytes(OP_CLOSURE, make_constant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalue_count; i++) {
        emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(compiler.upvalues[i].index);
    }
}

static void fun_declaration() {
    u8 global = parse_variable("Expect function name.");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
}

static void var_declaration() {
    u8 global = parse_variable("Expect variable name.");

    if (match_token(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }

    if (match_token(TOKEN_SEMICOLON)) {
    }

    define_variable(global);
}

static void expression_statement() {
    expression();
    if (match_token(TOKEN_SEMICOLON)) {
        emit_byte(OP_POP);
    }
}

static void for_statement() {
    begin_scope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    if (match_token(TOKEN_LET)) {
        var_declaration();
    } else if (match_token(TOKEN_SEMICOLON)) {
    } else {
        expression_statement();
    }

    int loop_start = current_chunk()->count;

    int exit_jump = -1;
    if (!match_token(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }

    if (!check(TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(OP_JUMP);
        int increment_start = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    } else {
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    }

    statement();
    emit_loop(loop_start);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }

    end_scope();
}

static void if_statement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();

    int else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);
    emit_byte(OP_POP);

    if (match_token(TOKEN_ELSE)) {
        statement();
    }
    patch_jump(else_jump);
}

static void return_statement() {
    if (current->type == TYPE_SCRIPT) {
        error("Cannot return from top-level code.");
    }

    if (match_token(TOKEN_SEMICOLON) || check(TOKEN_RIGHT_BRACE) || check(TOKEN_EOF)) {
        emit_return();
    } else {
        expression();
        if (match_token(TOKEN_SEMICOLON)) {}
        emit_byte(OP_RETURN);
    }
}

static void while_statement() {
    int loop_start = current_chunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

static void assert_statement() {
    expression();
    emit_byte(OP_ASSERT);
    if (match_token(TOKEN_SEMICOLON)) {}
}

static void synchronize() {
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_FN:
            case TOKEN_LET:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_RETURN:
                return;
            default:
                break;
        }
        advance();
    }
}

static void declaration() {
    if (match_token(TOKEN_FN)) {
        fun_declaration();
    } else if (match_token(TOKEN_LET)) {
        var_declaration();
    } else {
        statement();
    }

    if (parser.panic_mode) synchronize();
}

static void statement() {
    if (match_token(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else if (match_token(TOKEN_FOR)) {
        for_statement();
    } else if (match_token(TOKEN_IF)) {
        if_statement();
    } else if (match_token(TOKEN_RETURN)) {
        return_statement();
    } else if (match_token(TOKEN_WHILE)) {
        while_statement();
    } else if (match_token(TOKEN_ASSERT)) {
        assert_statement();
    } else {
        expression_statement();
    }
}

ObjFunction* compile(const char* source, InterpretResult* result) {
    Lexer lexer;
    lexer_init(&lexer, source);
    parser.lexer = &lexer;

    parser.had_error = false;
    parser.panic_mode = false;

    Compiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    advance();

    while (!match_token(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = end_compiler();

    if (parser.had_error) {
        if (result) *result = RESULT_COMPILE_ERROR;
        return NULL;
    }

    if (result) *result = RESULT_OK;
    return function;
}

void compiler_mark_roots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        gc_mark_object(&compiler->function->obj);
        gc_mark_array(compiler->function->chunk.constants.data,
                      compiler->function->chunk.constants.count);
        compiler = compiler->enclosing;
    }
}

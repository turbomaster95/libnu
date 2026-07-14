#include <nu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOK_EOF = 0,
    TOK_INT_LIT,
    TOK_IDENT,
    TOK_INT_KEYWORD,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_RETURN,
    TOK_ASSIGN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_LT,
    TOK_EQ,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMICOLON,
    TOK_MAX
} C99_Token_t;

typedef enum {
    PREC_NONE = 0,
    PREC_ASSIGN = 10,
    PREC_COMPARISON = 20,
    PREC_TERM = 30,
    PREC_FACTOR = 40,
} C99_Precedence_t;

typedef enum {
    AST_INT_LIT = 1,
    AST_IDENT,
    AST_BINARY_OP,
    AST_VAR_DECL,
    AST_ASSIGN,
    AST_IF,
    AST_WHILE,
    AST_BLOCK,
    AST_RETURN,
    AST_FUNC_DEF,
} C99_AST_Type_t;

static int reg_allocator = 0;
static int label_allocator = 0;

static bool is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') || (c >= '0' && c <= '9');
}

nu_token_t c99_next_token(nu_lexer_t *lexer) {
    nu_lexer_skip_whitespace(lexer);

    nu_token_t tok = {0};
    tok.line = lexer->line;
    tok.column = lexer->cursor - lexer->line_start;
    tok.text = &lexer->source[lexer->cursor];

    char c = nu_lexer_peek_char(lexer);
    if (c == '\0') {
        tok.type = TOK_EOF;
        tok.length = 0;
        return tok;
    }

    if (c >= '0' && c <= '9') {
        size_t start = lexer->cursor;
        while (c >= '0' && c <= '9') {
            nu_lexer_advance_char(lexer);
            c = nu_lexer_peek_char(lexer);
        }
        tok.type = TOK_INT_LIT;
        tok.length = lexer->cursor - start;
        return tok;
    }

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        size_t start = lexer->cursor;
        while (is_ident_char(c)) {
            nu_lexer_advance_char(lexer);
            c = nu_lexer_peek_char(lexer);
        }
        tok.length = lexer->cursor - start;

        nu_span_t span = nu_span_from_parts(tok.text, tok.length);
        if (nu_span_eq(span, "int"))         tok.type = TOK_INT_KEYWORD;
        else if (nu_span_eq(span, "if"))     tok.type = TOK_IF;
        else if (nu_span_eq(span, "else"))   tok.type = TOK_ELSE;
        else if (nu_span_eq(span, "while"))  tok.type = TOK_WHILE;
        else if (nu_span_eq(span, "return")) tok.type = TOK_RETURN;
        else                                 tok.type = TOK_IDENT;
        return tok;
    }

    if (c == '=') {
        nu_lexer_advance_char(lexer);
        if (nu_lexer_peek_char(lexer) == '=') {
            nu_lexer_advance_char(lexer);
            tok.type = TOK_EQ;
            tok.length = 2;
            return tok;
        }
        tok.type = TOK_ASSIGN;
        tok.length = 1;
        return tok;
    }

    nu_lexer_advance_char(lexer);
    tok.length = 1;
    switch (c) {
        case '+': tok.type = TOK_PLUS; break;
        case '-': tok.type = TOK_MINUS; break;
        case '*': tok.type = TOK_STAR; break;
        case '/': tok.type = TOK_SLASH; break;
        case '<': tok.type = TOK_LT; break;
        case '(': tok.type = TOK_LPAREN; break;
        case ')': tok.type = TOK_RPAREN; break;
        case '{': tok.type = TOK_LBRACE; break;
        case '}': tok.type = TOK_RBRACE; break;
        case ';': tok.type = TOK_SEMICOLON; break;
        default:
            exit(1);
    }
    return tok;
}

nu_ast_node_t* parse_statement(nu_parser_t *parser);
nu_ast_node_t* parse_block(nu_parser_t *parser);

static nu_ast_node_t* parse_primary_nud(nu_parser_t *parser, nu_token_t tok) {
    if (tok.type == TOK_INT_LIT) {
        nu_ast_node_t *node = nu_ast_new_node(parser->ast, AST_INT_LIT);
        char tmp[64] = {0};
        snprintf(tmp, sizeof(tmp) < tok.length + 1 ? sizeof(tmp) : tok.length + 1, "%s", tok.text);
        nu_ast_set_int(node, strtoll(tmp, NULL, 10));
        return node;
    }
    if (tok.type == TOK_IDENT) {
        nu_ast_node_t *node = nu_ast_new_node(parser->ast, AST_IDENT);
        nu_ast_set_str(parser->ast, node, tok.text, tok.length);
        return node;
    }
    if (tok.type == TOK_LPAREN) {
        nu_ast_node_t *expr = nu_parse_expression(parser, PREC_NONE);
        nu_parser_match(parser, TOK_RPAREN);
        return expr;
    }
    exit(1);
}

static nu_ast_node_t* parse_binary_led(nu_parser_t *parser, nu_ast_node_t *left, nu_token_t tok) {
    nu_ast_node_t *parent = nu_ast_new_node(parser->ast, AST_BINARY_OP);
    nu_ast_set_str(parser->ast, parent, tok.text, tok.length);

    uint32_t lbp = parser->rules[tok.type].lbp;
    nu_ast_node_t *right = nu_parse_expression(parser, lbp);

    nu_ast_add_child(parent, left);
    nu_ast_add_child(parent, right);
    return parent;
}

nu_ast_node_t* parse_block(nu_parser_t *parser) {
    nu_ast_node_t *block = nu_ast_new_node(parser->ast, AST_BLOCK);
    nu_parser_match(parser, TOK_LBRACE);

    while (parser->current.type != TOK_RBRACE && parser->current.type != TOK_EOF) {
        nu_ast_add_child(block, parse_statement(parser));
    }

    nu_parser_match(parser, TOK_RBRACE);
    return block;
}

nu_ast_node_t* parse_statement(nu_parser_t *parser) {
    if (parser->current.type == TOK_LBRACE) {
        return parse_block(parser);
    }

    if (nu_parser_match(parser, TOK_INT_KEYWORD)) {
        nu_token_t ident = parser->current;
        nu_parser_match(parser, TOK_IDENT);

        if (parser->current.type == TOK_LPAREN) {
            nu_parser_match(parser, TOK_LPAREN);
            nu_parser_match(parser, TOK_RPAREN);

            nu_ast_node_t *body = parse_block(parser);

            nu_ast_node_t *func = nu_ast_new_node(parser->ast, AST_FUNC_DEF);
            nu_ast_set_str(parser->ast, func, ident.text, ident.length);
            nu_ast_add_child(func, body);
            return func;
        }

        nu_ast_node_t *decl = nu_ast_new_node(parser->ast, AST_VAR_DECL);
        nu_ast_set_str(parser->ast, decl, ident.text, ident.length);

        if (nu_parser_match(parser, TOK_ASSIGN)) {
            nu_ast_node_t *init_val = nu_parse_expression(parser, PREC_NONE);
            nu_ast_add_child(decl, init_val);
        }
        nu_parser_match(parser, TOK_SEMICOLON);
        return decl;
    }

    if (parser->current.type == TOK_IDENT) {
        nu_token_t ident = parser->current;
        nu_parser_advance(parser);

        nu_parser_match(parser, TOK_ASSIGN);
        nu_ast_node_t *expr = nu_parse_expression(parser, PREC_NONE);
        nu_parser_match(parser, TOK_SEMICOLON);

        nu_ast_node_t *assign = nu_ast_new_node(parser->ast, AST_ASSIGN);
        nu_ast_set_str(parser->ast, assign, ident.text, ident.length);
        nu_ast_add_child(assign, expr);
        return assign;
    }

    if (nu_parser_match(parser, TOK_IF)) {
        nu_parser_match(parser, TOK_LPAREN);
        nu_ast_node_t *cond = nu_parse_expression(parser, PREC_NONE);
        nu_parser_match(parser, TOK_RPAREN);

        nu_ast_node_t *then_branch = parse_statement(parser);

        nu_ast_node_t *if_node = nu_ast_new_node(parser->ast, AST_IF);
        nu_ast_add_child(if_node, cond);
        nu_ast_add_child(if_node, then_branch);

        if (nu_parser_match(parser, TOK_ELSE)) {
            nu_ast_node_t *else_branch = parse_statement(parser);
            nu_ast_add_child(if_node, else_branch);
        }
        return if_node;
    }

    if (nu_parser_match(parser, TOK_WHILE)) {
        nu_parser_match(parser, TOK_LPAREN);
        nu_ast_node_t *cond = nu_parse_expression(parser, PREC_NONE);
        nu_parser_match(parser, TOK_RPAREN);

        nu_ast_node_t *body = parse_statement(parser);

        nu_ast_node_t *while_node = nu_ast_new_node(parser->ast, AST_WHILE);
        nu_ast_add_child(while_node, cond);
        nu_ast_add_child(while_node, body);
        return while_node;
    }

    if (nu_parser_match(parser, TOK_RETURN)) {
        nu_ast_node_t *ret_val = nu_parse_expression(parser, PREC_NONE);
        nu_parser_match(parser, TOK_SEMICOLON);

        nu_ast_node_t *ret_node = nu_ast_new_node(parser->ast, AST_RETURN);
        nu_ast_add_child(ret_node, ret_val);
        return ret_node;
    }

    exit(1);
}

int compile_to_ir(nu_ast_node_t *node) {
    if (!node) return -1;

    switch (node->type) {
        case AST_FUNC_DEF: {
            nu_printf("FUNC_START %s\n", node->val.str);
            compile_to_ir(node->first_child);
            nu_printf("FUNC_END\n");
            return -1;
        }
        case AST_INT_LIT: {
            int r = reg_allocator++;
            nu_printf("  t%d = LOAD_INT %lld\n", r, node->val.i64);
            return r;
        }
        case AST_IDENT: {
            int r = reg_allocator++;
            nu_printf("  t%d = LOAD_VAR %s\n", r, node->val.str);
            return r;
        }
        case AST_BINARY_OP: {
            int left_reg = compile_to_ir(node->first_child);
            int right_reg = compile_to_ir(node->first_child->next_sibling);
            int dest = reg_allocator++;

            const char *op = "ADD";
            if (strcmp(node->val.str, "-") == 0) op = "SUB";
            else if (strcmp(node->val.str, "*") == 0) op = "MUL";
            else if (strcmp(node->val.str, "/") == 0) op = "DIV";
            else if (strcmp(node->val.str, "<") == 0) op = "CMP_LT";
            else if (strcmp(node->val.str, "==") == 0) op = "CMP_EQ";

            nu_printf("  t%d = %s t%d, t%d\n", dest, op, left_reg, right_reg);
            return dest;
        }
        case AST_VAR_DECL: {
            if (node->first_child) {
                int r = compile_to_ir(node->first_child);
                nu_printf("  STORE %s, t%d\n", node->val.str, r);
            }
            return -1;
        }
        case AST_ASSIGN: {
            int r = compile_to_ir(node->first_child);
            nu_printf("  STORE %s, t%d\n", node->val.str, r);
            return -1;
        }
        case AST_BLOCK: {
            nu_ast_node_t *stmt = node->first_child;
            while (stmt) {
                compile_to_ir(stmt);
                stmt = stmt->next_sibling;
            }
            return -1;
        }
        case AST_IF: {
            int cond_reg = compile_to_ir(node->first_child);
            int else_lbl = label_allocator++;
            int end_lbl = label_allocator++;

            nu_printf("  JUMP_ZERO t%d, L%d\n", cond_reg, else_lbl);

            compile_to_ir(node->first_child->next_sibling);
            nu_printf("  JUMP L%d\n", end_lbl);

            nu_printf("L%d:\n", else_lbl);
            nu_ast_node_t *else_node = node->first_child->next_sibling->next_sibling;
            if (else_node) {
                compile_to_ir(else_node);
            }
            nu_printf("L%d:\n", end_lbl);
            return -1;
        }
        case AST_WHILE: {
            int start_lbl = label_allocator++;
            int end_lbl = label_allocator++;

            nu_printf("L%d:\n", start_lbl);
            int cond_reg = compile_to_ir(node->first_child);
            nu_printf("  JUMP_ZERO t%d, L%d\n", cond_reg, end_lbl);

            compile_to_ir(node->first_child->next_sibling);
            nu_printf("  JUMP L%d\n", start_lbl);
            nu_printf("L%d:\n", end_lbl);
            return -1;
        }
        case AST_RETURN: {
            int r = compile_to_ir(node->first_child);
            nu_printf("  RET t%d\n", r);
            return -1;
        }
    }
    return -1;
}

static const nu_parser_rule_t C99_Rules[TOK_MAX] = {
    [TOK_INT_LIT] = { .nud = parse_primary_nud, .led = NULL,             .lbp = PREC_NONE },
    [TOK_IDENT]   = { .nud = parse_primary_nud, .led = NULL,             .lbp = PREC_NONE },
    [TOK_LPAREN]  = { .nud = parse_primary_nud, .led = NULL,             .lbp = PREC_NONE },
    [TOK_PLUS]    = { .nud = NULL,              .led = parse_binary_led, .lbp = PREC_TERM },
    [TOK_MINUS]   = { .nud = NULL,              .led = parse_binary_led, .lbp = PREC_TERM },
    [TOK_STAR]    = { .nud = NULL,              .led = parse_binary_led, .lbp = PREC_FACTOR },
    [TOK_SLASH]   = { .nud = NULL,              .led = parse_binary_led, .lbp = PREC_FACTOR },
    [TOK_LT]      = { .nud = NULL,              .led = parse_binary_led, .lbp = PREC_COMPARISON },
    [TOK_EQ]      = { .nud = NULL,              .led = parse_binary_led, .lbp = PREC_COMPARISON },
};

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source.i>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *source_code = malloc(size + 1);
    if (!source_code) {
        fclose(f);
        return 1;
    }

    size_t read_bytes = fread(source_code, 1, size, f);
    source_code[read_bytes] = '\0';
    fclose(f);

    size_t mem_size = 4 * 1024 * 1024;
    void *backing_mem = malloc(mem_size);
    nu_mm_t *arena = nu_mm_create(NU_MM_ARENA, backing_mem, mem_size);

    nu_lexer_t lexer;
    nu_lexer_init(&lexer, source_code);

    nu_ast_t *ast = nu_ast_create(arena);

    nu_parser_t parser;
    nu_parser_init(&parser, &lexer, ast, c99_next_token, C99_Rules, TOK_MAX);
    nu_parser_advance(&parser);

    nu_ast_node_t *root_node = NULL;
    if (parser.current.type == TOK_LBRACE) {
        root_node = parse_block(&parser);
    } else {
        root_node = parse_statement(&parser);
    }

    compile_to_ir(root_node);

    nu_ast_destroy(ast);
    nu_mm_destroy(arena);
    free(backing_mem);
    free(source_code);

    return 0;
}


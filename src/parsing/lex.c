#include <nu.h>

void nu_lexer_init(nu_lexer_t *lexer, const char *source) {
    if (!lexer || !source) return;
    lexer->source = source;
    lexer->cursor = 0;
    lexer->line = 1;
    lexer->line_start = 0;
}

char nu_lexer_peek_char(nu_lexer_t *lexer) {
    if (!lexer || !lexer->source) return '\0';
    return lexer->source[lexer->cursor];
}

char nu_lexer_advance_char(nu_lexer_t *lexer) {
    if (!lexer || !lexer->source) return '\0';
    char c = lexer->source[lexer->cursor];
    if (c == '\0') return '\0';
    lexer->cursor++;
    if (c == '\n') {
        lexer->line++;
        lexer->line_start = lexer->cursor;
    }
    return c;
}

void nu_lexer_skip_whitespace(nu_lexer_t *lexer) {
    if (!lexer) return;
    while (1) {
        char c = nu_lexer_peek_char(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            nu_lexer_advance_char(lexer);
        } else {
            break;
        }
    }
}


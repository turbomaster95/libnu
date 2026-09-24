#include <nu.h>
#include <stdint.h>

void nu_parser_init(nu_parser_t *parser, nu_lexer_t *lexer, nu_ast_t *ast, nu_token_t (*next_token_cb)(nu_lexer_t *), const nu_parser_rule_t *rules, uint32_t max_token_id) {
    if (!parser) {
        return;
    }

    parser->lexer = lexer;
    parser->ast = ast;
    parser->next_token_cb = next_token_cb;
    parser->rules = rules;
    parser->max_token_id = max_token_id;

    parser->current.type = 0;
    parser->peek.type = 0;

    if (!lexer || !next_token_cb) {
        return;
    }

    parser->current = next_token_cb(lexer);
    parser->peek = next_token_cb(lexer);
}

void nu_parser_advance(nu_parser_t *parser) {
    if (!parser || !parser->lexer || !parser->next_token_cb) {
        return;
    }

    parser->current = parser->peek;
    parser->peek = parser->next_token_cb(parser->lexer);
}

bool nu_parser_match(nu_parser_t *parser, uint32_t expected_type) {
    if (!parser) {
        return false;
    }

    if (parser->current.type != expected_type) {
        return false;
    }

    nu_parser_advance(parser);
    return true;
}

nu_ast_node_t *nu_parse_expression(nu_parser_t *parser, uint32_t binding_power) {
    if (!parser || !parser->rules || !parser->lexer ||
        !parser->next_token_cb) {
        return NULL;
    }

    nu_token_t tok = parser->current;

    if (tok.type > parser->max_token_id) {
        return NULL;
    }

    nu_nud_fn nud = parser->rules[tok.type].nud;

    if (!nud) {
        return NULL;
    }

    nu_parser_advance(parser);

    nu_ast_node_t *left = nud(parser, tok);

    if (!left) {
        return NULL;
    }

    while (1) {
        nu_token_t next_tok = parser->current;

        if (next_tok.type > parser->max_token_id) {
            break;
        }

        uint32_t next_lbp = parser->rules[next_tok.type].lbp;

        if (binding_power >= next_lbp) {
            break;
        }

        nu_led_fn led = parser->rules[next_tok.type].led;

        if (!led) {
            break;
        }

        nu_parser_advance(parser);

        left = led(parser, left, next_tok);

        if (!left) {
            return NULL;
        }
    }

    return left;
}

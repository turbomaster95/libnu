#include <nu.h>
#include <stdint.h>

void nu_parser_init(nu_parser_t *parser, nu_lexer_t *lexer, nu_ast_t *ast, nu_token_t (*next_token_cb)(nu_lexer_t *), const nu_parser_rule_t *rules, uint32_t max_token_id) {
    parser->lexer = lexer;
    parser->ast = ast;
    parser->next_token_cb = next_token_cb;
    parser->rules = rules;
    parser->max_token_id = max_token_id;

    /* Pre-load lookahead step tracks */
    parser->current = parser->next_token_cb(parser->lexer);
    parser->peek = parser->next_token_cb(parser->lexer);
}

void nu_parser_advance(nu_parser_t *parser) {
    parser->current = parser->peek;
    parser->peek = parser->next_token_cb(parser->lexer);
}

bool nu_parser_match(nu_parser_t *parser, uint32_t expected_type) {
    if (parser->current.type == expected_type) {
        nu_parser_advance(parser);
        return true;
    }
    return false;
}

nu_ast_node_t* nu_parse_expression(nu_parser_t *parser, uint32_t binding_power) {
    nu_token_t tok = parser->current;
    if (tok.type > parser->max_token_id) return NULL;

    nu_nud_fn nud = parser->rules[tok.type].nud;
    if (!nud) return NULL; /* Syntax Err: Expression cannot begin with this fucking token */

    nu_parser_advance(parser);
    nu_ast_node_t *left = nud(parser, tok);

    while (1) {
        nu_token_t next_tok = parser->current;
        if (next_tok.type > parser->max_token_id) break;

        uint32_t next_lbp = parser->rules[next_tok.type].lbp;
        if (binding_power >= next_lbp) break;

        nu_led_fn led = parser->rules[next_tok.type].led;
        if (!led) break;

        nu_parser_advance(parser);
        left = led(parser, left, next_tok);
    }
    return left;
}


#include <nu.h>
#include <limits.h>

static int64_t nu_internal_atoi(const char *s, bool *success) {
    int64_t res = 0;
    int sign = 1;
    *success = false;

    if (!s || !*s) return 0;

    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    if (!*s) return 0;

    while (*s) {
        if (*s < '0' || *s > '9') return 0;

        int digit = *s - '0';

        if (sign == 1) {
            if (res > (INT64_MAX - digit) / 10) return 0;
            res = (res * 10) + digit;
        } else {
            if (res < (INT64_MIN + digit) / 10) return 0;
            res = (res * 10) - digit;
        }
        s++;
    }

    *success = true;
    return res;
}

nu_arg_parser_t* nu_arg_create(nu_mm_t *mm) {
    nu_arg_parser_t *ap = (nu_arg_parser_t*)nu_alloc(mm, sizeof(nu_arg_parser_t));
    if (!ap) return NULL;

    ap->mm = mm;
    ap->flag_trie = nu_trie_create(mm);
    if (!ap->flag_trie) {
        nu_free(mm, ap);
        return NULL;
    }

    ap->positional_count = 0;
    ap->positional_cap = 4;
    ap->positionals = (const char**)nu_alloc(mm, sizeof(char*) * ap->positional_cap);
    if (!ap->positionals) {
        nu_trie_destroy(ap->flag_trie);
        nu_free(mm, ap);
        return NULL;
    }

    ap->program_name = "program";
    return ap;
}

bool nu_arg_register(nu_arg_parser_t *ap, nu_arg_def_t *def) {
    if (!ap || !def) return false;

    def->is_set = false;
    if (def->type == NU_ARG_BOOL) def->val.b = false;
    else if (def->type == NU_ARG_INT) def->val.i = 0;
    else def->val.s = NULL;

    bool ok = true;
    if (def->short_flag && *def->short_flag) {
        ok &= nu_trie_insert(ap->flag_trie, def->short_flag, def);
    }
    if (def->long_flag && *def->long_flag) {
        ok &= nu_trie_insert(ap->flag_trie, def->long_flag, def);
    }
    return ok;
}

bool nu_arg_parse(nu_arg_parser_t *ap, int argc, char *argv[]) {
    if (!ap || argc <= 0 || !argv) return false;

    if (argv[0] && argv[0][0] != '\0') {
        ap->program_name = argv[0];
    }

    bool dash_dash_seen = false;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (!arg) continue;

        if (!dash_dash_seen && arg[0] == '-' && arg[1] == '-' && arg[2] == '\0') {
            dash_dash_seen = true;
            continue;
        }

        if (!dash_dash_seen && arg[0] == '-' && arg[1] != '\0') {
            nu_arg_def_t *def = (nu_arg_def_t*)nu_trie_lookup(ap->flag_trie, arg);
            if (!def) {
                nu_printf("Error: Unknown argument flag option '%s'\n", arg);
                return false;
            }

            def->is_set = true;

            if (def->type == NU_ARG_BOOL) {
                def->val.b = true;
            } else {
                if (i + 1 >= argc) {
                    nu_printf("Error: Argument option '%s' requires an associated value parameter\n", arg);
                    return false;
                }
                char *val_str = argv[++i];

                if (def->type == NU_ARG_STR) {
                    def->val.s = val_str;
                } else if (def->type == NU_ARG_INT) {
                    bool valid_int = false;
                    def->val.i = nu_internal_atoi(val_str, &valid_int);
                    if (!valid_int) {
                        nu_printf("Error: Value '%s' passed to flag option '%s' is not a valid integer/out of bounds\n", val_str, arg);
                        return false;
                    }
                }
            }
        } else {
            if (ap->positional_count >= ap->positional_cap) {
                size_t new_cap = ap->positional_cap * 2;
                const char **new_pos = (const char**)nu_alloc(ap->mm, sizeof(char*) * new_cap);
                if (!new_pos) {
                    nu_printf("Error: Out of memory expanding positional argument array\n");
                    return false;
                }

                for (size_t k = 0; k < ap->positional_count; k++) {
                    new_pos[k] = ap->positionals[k];
                }
                nu_free(ap->mm, (void*)ap->positionals);
                ap->positionals = new_pos;
                ap->positional_cap = new_cap;
            }
            ap->positionals[ap->positional_count++] = arg;
        }
    }
    return true;
}

void nu_arg_print_help(nu_arg_parser_t *ap, nu_arg_def_t defs[], size_t def_count) {
    if (!ap || !defs) return;

    nu_printf("Usage: %s [options]\n\nOptions:\n", ap->program_name ? ap->program_name : "program");
    for (size_t i = 0; i < def_count; i++) {
        nu_arg_def_t d = defs[i];
        char flag_buf[64] = {0};

        if (d.short_flag && d.long_flag) {
            nu_snprintf(flag_buf, sizeof(flag_buf), "  %s, %s", d.short_flag, d.long_flag);
        } else if (d.long_flag) {
            nu_snprintf(flag_buf, sizeof(flag_buf), "      %s", d.long_flag);
        } else if (d.short_flag) {
            nu_snprintf(flag_buf, sizeof(flag_buf), "  %s", d.short_flag);
        }

        nu_printf("%-24s %s\n", flag_buf, d.help ? d.help : "");
    }
}

void nu_arg_destroy(nu_arg_parser_t *ap) {
    if (!ap) return;
    if (ap->flag_trie) nu_trie_destroy(ap->flag_trie);
    if (ap->positionals) nu_free(ap->mm, (void*)ap->positionals);
    nu_free(ap->mm, ap);
}


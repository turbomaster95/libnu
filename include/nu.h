#ifndef LIBNU_H
#define LIBNU_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef NUO_H

#define NU_SMASH_CFG()     _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_CFG()")
#define NU_SMASH_DATA(var) _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_DATA()")
#define NU_SMASH_FLAGS()   _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_FLAGS()")
#define NU_SMASH_REGS()    _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_REGS()")
#define NU_SMASH_SIMD()    _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_SIMD()")
#define nu_obfs(...)       _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use nu_obfs()")

#endif /* NUO_H */

typedef enum {
    NU_LOG_DEBUG,
    NU_LOG_INFO,
    NU_LOG_WARN,
    NU_LOG_ERROR
} nu_log_level_t;

typedef void (*nu_proc_io_cb)(const char *buffer, size_t len, void *data);

void nu_log_output(nu_log_level_t level, const char *file, int line, const char *fmt, ...);

#define NU_DEBUG(fmt, ...) nu_log_output(NU_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NU_INFO(fmt, ...)  nu_log_output(NU_LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NU_WARN(fmt, ...)  nu_log_output(NU_LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NU_ERROR(fmt, ...) nu_log_output(NU_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

typedef struct nu_mm nu_mm_t;

typedef enum {
    NU_MM_ARENA,
    NU_MM_BUDDY,
    NU_MM_SLAB,
    NU_MM_SLOB
} nu_mm_type_t;

// Creates a new nu_mm instance with the specified type,
// backing memory (ptr. to the memory that is supposed to be managed) and size of the memory.
nu_mm_t* nu_mm_create(nu_mm_type_t type, void *backing_mem, size_t size);

// Destroys a nu_mm instance
void nu_mm_destroy(nu_mm_t *mm);

// Alloc and Free for a specific instance.
void* nu_alloc(nu_mm_t *mm, size_t size);
void  nu_free(nu_mm_t *mm, void *ptr);

typedef struct nu_map nu_map_t;

// Creates a new hashmap using a specific allocator instance.
// arg1: The backing memory manager instance to handle allocations.
// arg2: Baseline slot count (must be a power of 2 for optimization).
nu_map_t* nu_map_create(nu_mm_t *mm, size_t initial_capacity);

// Tears down the map table and frees duplicated keys.
void nu_map_destroy(nu_map_t *map);

// Inserts or updates a key-value association.
// Automatically doubles table size when the load factor hits 70%.
bool nu_map_set(nu_map_t *map, const char *key, void *value);

// Looks up a key and returns its associated pointer value.
// Returns NULL if the key is not found.
void* nu_map_get(nu_map_t *map, const char *key);

// Removes a key from the map
bool nu_map_delete(nu_map_t *map, const char *key);

// Returns total number of active elements inside the map.
size_t nu_map_size(nu_map_t *map);

// Raw 64-bit block operations
void nu_xtea_encrypt_block(const uint32_t key[4], uint32_t block[2]);
void nu_xtea_decrypt_block(const uint32_t key[4], uint32_t block[2]);

// Derives a valid 128-bit key out of any string passphrase
void nu_xtea_derive_key(const char *passphrase, uint32_t key[4]);

// CTR Stream Mode.
// Handles arbitrary data lengths
// Passing encrypted data through this function decrypts it.
// Requires an 8-byte nonce/initialization vector (use a random number or counter).
void nu_xtea_crypt_ctr(const uint32_t key[4], uint64_t nonce, uint8_t *data, size_t size);

typedef struct {
    uint32_t type;         /* User-defined token type identifier */
    const char *text;      /* Direct pointer into source buffer string slice */
    size_t length;         /* Length of the token string slice */
    size_t line;           /* Line number tracker for diagnostics */
    size_t column;         /* Precise column start index for error pointers */
} nu_token_t;

typedef struct {
    const char *source;    /* Target code text buffer */
    size_t cursor;         /* Read index offset */
    size_t line;           /* Current line index tracker */
    size_t line_start;     /* Index offset where current line started */
} nu_lexer_t;

// Lexer Functions
void nu_lexer_init(nu_lexer_t *lexer, const char *source);
char nu_lexer_peek_char(nu_lexer_t *lexer);
char nu_lexer_advance_char(nu_lexer_t *lexer);
void nu_lexer_skip_whitespace(nu_lexer_t *lexer);

typedef struct nu_ast_node nu_ast_node_t;

typedef union {
    int64_t i64;
    double f64;
    const char *str;
    void *raw;
} nu_ast_val_t;

struct nu_ast_node {
    uint32_t type;                     /* User-defined AST node variant type */
    nu_ast_val_t val;                  /* Multi-type data storage variant slot */
    nu_ast_node_t *first_child;        /* Head of children sibling sequence chain */
    nu_ast_node_t *last_child;         /* Tail of children sequence chain */
    nu_ast_node_t *next_sibling;       /* Linked list pointer to adjacent scope node */
};

typedef struct {
    nu_mm_t *mm;                       /* Memory manager instance */
    nu_ast_node_t *root;               /* AST Entry execution target root */
} nu_ast_t;

// AST (Abstract Syntax Tree) Functions.
nu_ast_t* nu_ast_create(nu_mm_t *mm);
nu_ast_node_t* nu_ast_new_node(nu_ast_t *ast, uint32_t type);
nu_ast_node_t* nu_ast_new_branch(nu_ast_t *ast, uint32_t type, size_t child_count, ...);
void nu_ast_add_child(nu_ast_node_t *parent, nu_ast_node_t *child);
void nu_ast_set_str(nu_ast_t *ast, nu_ast_node_t *node, const char *str, size_t len);
void nu_ast_set_int(nu_ast_node_t *node, int64_t val);
void nu_ast_set_float(nu_ast_node_t *node, double val);
void nu_ast_destroy(nu_ast_t *ast);

typedef struct nu_parser nu_parser_t;

typedef nu_ast_node_t* (*nu_nud_fn)(nu_parser_t *parser, nu_token_t tok);
typedef nu_ast_node_t* (*nu_led_fn)(nu_parser_t *parser, nu_ast_node_t *left, nu_token_t tok);

typedef struct {
    nu_nud_fn nud;          /* Null Denotation handler (Prefix / Literals / Unary) */
    nu_led_fn led;          /* Left Denotation handler (Infix / Postfix operators) */
    uint32_t lbp;           /* Left Binding Power value (Precedence hierarchy weight) */
} nu_parser_rule_t;

struct nu_parser {
    nu_lexer_t *lexer;
    nu_ast_t *ast;
    nu_token_t current;
    nu_token_t peek;
    nu_token_t (*next_token_cb)(nu_lexer_t *lexer);
    const nu_parser_rule_t *rules;
    uint32_t max_token_id;
};

// Pratt Parser funcs.
void nu_parser_init(nu_parser_t *parser, nu_lexer_t *lexer, nu_ast_t *ast, nu_token_t (*next_token_cb)(nu_lexer_t *), const nu_parser_rule_t *rules, uint32_t max_token_id);
void nu_parser_advance(nu_parser_t *parser);
bool nu_parser_match(nu_parser_t *parser, uint32_t expected_type);
nu_ast_node_t* nu_parse_expression(nu_parser_t *parser, uint32_t binding_power);

// Memory-safe string split. Returns heap-allocated array of strings. Free with nu_str_free_list.
char** nu_str_split(const char *str, const char *delim, int *out_count);
void   nu_str_free_list(char **list, int count);

// Trims whitespace in-place (modifies the buffer)
char* nu_str_trim(char *str);

// Quick JSON string extractor. Extracts value matching a top-level or dotted key.
// Caller must free() returned string. Returns NULL if not found.
char* nu_json_extract(const char *json, const char *key);

typedef struct nu_loop nu_loop_t;
typedef void (*nu_event_cb)(int fd, void *data);

nu_loop_t* nu_loop_create(void);
void       nu_loop_destroy(nu_loop_t *loop);

// Add a standard file descriptor to watch for readability
bool       nu_loop_add_fd(nu_loop_t *loop, int fd, nu_event_cb cb, void *data);

// Add a timer (interval in milliseconds)
bool       nu_loop_add_timer(nu_loop_t *loop, int ms, nu_event_cb cb, void *data);

// Watch a file/directory path for modifications
bool       nu_loop_add_watch(nu_loop_t *loop, const char *path, nu_event_cb cb, void *data);

// Run the loop indefinitely. Returns false on catastrophic failure.
bool       nu_loop_run(nu_loop_t *loop);

typedef void (*nu_ipc_cb)(const char *msg, int client_fd, void *data);

// Starts a blocking IPC server. Typically spawned in its own thread or used as a standalone daemon.
// Automatically creates a UNIX socket at 'sock_path'.
void       nu_ipc_listen(const char *sock_path, nu_ipc_cb cb, void *data);

// Sends a quick message to a listening IPC server and returns response (or NULL)
char* nu_ipc_send(const char *sock_path, const char *message);

// Mask and monitor system signals via epoll loop.
// Pass signum (e.g., SIGINT, SIGTERM). Returns false on failure.
bool nu_loop_add_signal(nu_loop_t *loop, int signum, nu_event_cb cb, void *data);

// Exec background processes without hanging. Pipeline matches standard argv format.
bool nu_process_spawn(nu_loop_t *loop, char *const argv[], nu_proc_io_cb stdout_cb, void *data);

// Daemonize easily
bool nu_daemonize(void);

#endif // LIBNU_H

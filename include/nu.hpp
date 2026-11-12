#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// libnu exposes a C API. Give its declarations C linkage when included from C++.
#if defined(_MSC_VER)
    #define NU_HPP_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
    #define NU_HPP_RESTRICT __restrict__
#else
    #define NU_HPP_RESTRICT
#endif

#ifndef restrict
    #define restrict NU_HPP_RESTRICT
    #define NU_HPP_DEFINED_RESTRICT
#endif

extern "C" {
    #include <nu.h>
    #include <nus.h>

    nu_calc_t nu_calc_div(nu_calc_t a, nu_calc_t b);
}

#ifdef NU_HPP_DEFINED_RESTRICT
    #undef restrict
    #undef NU_HPP_DEFINED_RESTRICT
#endif

#undef NU_HPP_RESTRICT

namespace nu {
    inline uint64_t time_now() { return nu_time_now(); }
    inline uint64_t time_monotonic() { return nu_time_monotonic(); }
    inline void sleep_ns(uint64_t ns) { nu_sleep_ns(ns); }
    inline bool daemonize() { return nu_daemonize(); }

    namespace memory {

        using Type = nu_mm_type_t;

        constexpr Type Arena = NU_MM_ARENA;
        constexpr Type Buddy = NU_MM_BUDDY;
        constexpr Type Slab = NU_MM_SLAB;
        constexpr Type Slob = NU_MM_SLOB;

        class Manager {
        private:
            nu_mm_t *mm = nullptr;

        public:
            Manager(Type type, void *backing, size_t size) : mm(nu_mm_create(type, backing, size)) {}
            ~Manager() { if (mm) nu_mm_destroy(mm); }

            Manager(const Manager &) = delete;
            Manager &operator=(const Manager &) = delete;

            Manager(Manager &&other) noexcept : mm(std::exchange(other.mm, nullptr)) {}

            Manager &operator=(Manager &&other) noexcept {
                if (this != &other) {
                    if (mm) nu_mm_destroy(mm);
                    mm = std::exchange(other.mm, nullptr);
                }
                return *this;
            }

            void *allocate(size_t size) { return mm ? nu_alloc(mm, size) : nullptr; }
            void deallocate(void *ptr) { if (mm) nu_free(mm, ptr); }
            void *reallocate(void *ptr, size_t size) { return mm ? nu_realloc(mm, ptr, size) : nullptr; }
            nu_mm_t *native() const { return mm; }
            explicit operator bool() const { return mm != nullptr; }
        };

    } // namespace memory

    namespace map {

        using Iterator = nu_map_iter_t;

        class Map {
        private:
            nu_map_t *handle = nullptr;

        public:
            Map(memory::Manager &mm, size_t capacity = 16) : handle(nu_map_create(mm.native(), capacity)) {}
            ~Map() { if (handle) nu_map_destroy(handle); }

            Map(const Map &) = delete;
            Map &operator=(const Map &) = delete;

            Map(Map &&other) noexcept : handle(std::exchange(other.handle, nullptr)) {}

            bool set(const char *key, void *value) { return handle && nu_map_set(handle, key, value); }
            bool set(const std::string &key, void *value) { return set(key.c_str(), value); }
            void *get(const char *key) const { return handle ? nu_map_get(handle, key) : nullptr; }
            void *get(const std::string &key) const { return get(key.c_str()); }
            bool contains(const char *key) const { return get(key) != nullptr; }
            bool erase(const char *key) { return handle && nu_map_delete(handle, key); }
            size_t size() const { return handle ? nu_map_size(handle) : 0; }
            bool empty() const { return size() == 0; }
            nu_map_t *native() const { return handle; }

            Iterator iterator() const {
                Iterator it{};
                if (handle) nu_map_iter_init(handle, &it);
                return it;
            }

            bool next(Iterator &it, const char **key, void **value) const {
                return nu_map_iter_next(&it, key, value);
            }
        };

    } // namespace map

    namespace trie {

        using Node = nu_trie_node_t;

        class Trie {
        private:
            nu_trie_t *handle = nullptr;

        public:
            explicit Trie(memory::Manager &mm) : handle(nu_trie_create(mm.native())) {}
            ~Trie() { if (handle) nu_trie_destroy(handle); }

            Trie(const Trie &) = delete;
            Trie &operator=(const Trie &) = delete;

            bool insert(const char *key, void *value) { return handle && nu_trie_insert(handle, key, value); }
            bool insert(const std::string &key, void *value) { return insert(key.c_str(), value); }
            void *lookup(const char *key) const { return handle ? nu_trie_lookup(handle, key) : nullptr; }
            void *lookup(const std::string &key) const { return lookup(key.c_str()); }
            bool erase(const char *key) { return handle && nu_trie_delete(handle, key); }
            size_t size() const { return handle ? handle->size : 0; }
            nu_trie_t *native() const { return handle; }
        };

    } // namespace trie

    namespace ast {

        using Node = nu_ast_node_t;
        using Value = nu_ast_val_t;
        using Tree = nu_ast_t;

        constexpr uint32_t JSON_NULL = NU_AST_JSON_NULL;
        constexpr uint32_t JSON_BOOL = NU_AST_JSON_BOOL;
        constexpr uint32_t JSON_INT = NU_AST_JSON_INT;
        constexpr uint32_t JSON_FLOAT = NU_AST_JSON_FLOAT;
        constexpr uint32_t JSON_STRING = NU_AST_JSON_STRING;
        constexpr uint32_t JSON_ARRAY = NU_AST_JSON_ARRAY;
        constexpr uint32_t JSON_OBJECT = NU_AST_JSON_OBJECT;
        constexpr uint32_t JSON_PAIR = NU_AST_JSON_PAIR;

        class AST {
        private:
            nu_ast_t *handle = nullptr;

        public:
            explicit AST(memory::Manager &mm) : handle(nu_ast_create(mm.native())) {}
            ~AST() { if (handle) nu_ast_destroy(handle); }

            AST(const AST &) = delete;
            AST &operator=(const AST &) = delete;

            Node *create(uint32_t type) { return handle ? nu_ast_new_node(handle, type) : nullptr; }

            template <typename... Children>
            [[nodiscard]] Node *branch(uint32_t type, size_t count, Children... children) {
                static_assert((std::is_convertible_v<Children, Node *> && ...), "AST branch children must be Node pointers");
                if (!handle || count != sizeof...(children)) return nullptr;
                return nu_ast_new_branch(handle, type, count, static_cast<Node *>(children)...);
            }

            void add(Node *parent, Node *child) { if (parent && child) nu_ast_add_child(parent, child); }
            void set(Node *node, const char *value, size_t length) { if (handle && node) nu_ast_set_str(handle, node, value, length); }
            void set(Node *node, const std::string &value) { set(node, value.c_str(), value.size()); }
            void set(Node *node, int64_t value) { if (node) nu_ast_set_int(node, value); }
            void set(Node *node, double value) { if (node) nu_ast_set_float(node, value); }

            Node *root() const { return handle ? handle->root : nullptr; }
            Tree *native() const { return handle; }
        };

    } // namespace ast

    class Json {
    public:
        using Node = nu_ast_node_t;

        static char *encode(memory::Manager &mm, const Node *root) {
            return nu_json_encode(mm.native(), root);
        }

        static Node *decode(memory::Manager &mm, const char *json) {
            return nu_json_decode(mm.native(), json);
        }

        static Node *decode(memory::Manager &mm, const std::string &json) {
            return decode(mm, json.c_str());
        }

        static Node *get(const Node *root, const char *keypath) {
            return nu_json_get(root, keypath);
        }

        static Node *get(const Node *root, const std::string &keypath) {
            return get(root, keypath.c_str());
        }
    };

    class Hash {
    public:
        enum class Algorithm {
            MD5 = NU_HASH_MD5,
            SHA1 = NU_HASH_SHA1,
            SHA256 = NU_HASH_SHA256,
            SHA512 = NU_HASH_SHA512
        };

        static char *encode(memory::Manager &mm, Algorithm algorithm, const uint8_t *data, size_t length) {
            return nu_hash_encode(mm.native(), static_cast<nu_hash_type_t>(algorithm), data, length);
        }

        static char *encode(memory::Manager &mm, Algorithm algorithm, const std::string &data) {
            return encode(mm, algorithm, reinterpret_cast<const uint8_t *>(data.data()), data.size());
        }
    };

    namespace crypto {
        namespace xtea {

            using Key = uint32_t[4];
            using Block = uint32_t[2];

            inline void encrypt_block(const uint32_t key[4], uint32_t block[2]) {
                nu_xtea_encrypt_block(key, block);
            }

            inline void decrypt_block(const uint32_t key[4], uint32_t block[2]) {
                nu_xtea_decrypt_block(key, block);
            }

            inline void derive_key(const char *passphrase, uint32_t key[4]) {
                nu_xtea_derive_key(passphrase, key);
            }

            inline void derive_key(const std::string &passphrase, uint32_t key[4]) {
                derive_key(passphrase.c_str(), key);
            }

            inline void crypt_ctr(const uint32_t key[4], uint64_t nonce, uint8_t *data, size_t size) {
                nu_xtea_crypt_ctr(key, nonce, data, size);
            }

            inline void crypt_ctr(const uint32_t key[4], uint64_t nonce, std::vector<uint8_t> &data) {
                crypt_ctr(key, nonce, data.data(), data.size());
            }

        } // namespace xtea
    } // namespace crypto

    namespace compression {
        namespace lz4 {

            inline size_t compress_bound(size_t size) {
                return nu_lz4_compress_bound(size);
            }

            inline size_t compress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size) {
                return nu_lz4_compress(src, src_size, dst, dst_size);
            }

            inline size_t decompress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size) {
                return nu_lz4_decompress(src, src_size, dst, dst_size);
            }

            inline std::vector<uint8_t> compress(const std::vector<uint8_t> &src) {
                std::vector<uint8_t> dst(compress_bound(src.size()));
                size_t size = compress(src.data(), src.size(), dst.data(), dst.size());
                if (!size) return {};
                dst.resize(size);
                return dst;
            }

            inline std::vector<uint8_t> decompress(const std::vector<uint8_t> &src, size_t expected_size) {
                std::vector<uint8_t> dst(expected_size);
                size_t size = decompress(src.data(), src.size(), dst.data(), dst.size());
                if (!size && expected_size != 0) return {};
                dst.resize(size);
                return dst;
            }

        } // namespace lz4
    } // namespace compression

    namespace calc {

        using Value = nu_calc_t;
        using Precision = nu_prec_type_t;
        using UInt128 = nu_u128;
        using UInt256 = nu_u256;

        constexpr Precision PREC_64 = NU_PREC_64;
        constexpr Precision PREC_128 = NU_PREC_128;
        constexpr Precision PREC_256 = NU_PREC_256;

        inline Value from_u64(uint64_t value) {
            return nu_calc_from_u64(value);
        }

        inline Value from_string(const char *value, int base = 10) {
            return nu_calc_from_str(value, base);
        }

        inline Value from_string(const std::string &value, int base = 10) {
            return from_string(value.c_str(), base);
        }

        inline Value add(Value a, Value b) { return nu_calc_add(a, b); }
        inline Value subtract(Value a, Value b) { return nu_calc_sub(a, b); }
        inline Value multiply(Value a, Value b) { return nu_calc_mul(a, b); }
        inline Value divide(Value a, Value b) { return nu_calc_div(a, b); }

        inline void print(Value value) { nu_calc_print(value); }

        inline Value operator+(Value a, Value b) { return add(a, b); }
        inline Value operator-(Value a, Value b) { return subtract(a, b); }
        inline Value operator*(Value a, Value b) { return multiply(a, b); }
        inline Value operator/(Value a, Value b) { return divide(a, b); }

    } // namespace calc

    class String {
    public:
        using Span = nu_span_t;

        static char **split(const char *str, const char *delimiter, int *count) {
            return nu_str_split(str, delimiter, count);
        }

        static char **split(const std::string &str, const std::string &delimiter, int *count) {
            return split(str.c_str(), delimiter.c_str(), count);
        }

        static void free_list(char **list, int count) {
            nu_str_free_list(list, count);
        }

        static char *trim(char *str) {
            return nu_str_trim(str);
        }

        static std::string trim(const std::string &str) {
            std::string result = str;
            size_t first = result.find_first_not_of(" \t\n\r\f\v");
            if (first == std::string::npos) return {};
            size_t last = result.find_last_not_of(" \t\n\r\f\v");
            return result.substr(first, last - first + 1);
        }

        static Span span(const char *str) {
            return nu_span_from_str(str);
        }

        static Span span(const char *ptr, size_t length) {
            return nu_span_from_parts(ptr, length);
        }

        static bool equals(Span value, const char *str) {
            return nu_span_eq(value, str);
        }

        static bool equals(Span a, Span b) {
            return nu_span_eq_span(a, b);
        }

        static Span trim(Span value) {
            return nu_span_trim(value);
        }

        static Span split_next(Span &value, char delimiter) {
            return nu_span_split_next(&value, delimiter);
        }
    };

    namespace buffer {

        using Buffer = nu_buf_t;

        class Dynamic {
        private:
            nu_buf_t buf{};

        public:
            Dynamic(memory::Manager &mm, size_t capacity = 64) {
                nu_buf_init(&buf, mm.native(), capacity);
            }

            ~Dynamic() { nu_buf_free(&buf); }

            Dynamic(const Dynamic &) = delete;
            Dynamic &operator=(const Dynamic &) = delete;

            void reserve(size_t capacity) { nu_buf_reserve(&buf, capacity); }
            void append(const void *data, size_t size) { nu_buf_append(&buf, data, size); }
            void append(uint8_t byte) { nu_buf_append_val(&buf, byte); }
            void reset() { nu_buf_reset(&buf); }
            void clear() { reset(); }
            void *data() { return buf.data; }
            const void *data() const { return buf.data; }
            size_t size() const { return buf.size; }
            size_t capacity() const { return buf.capacity; }
            bool empty() const { return buf.size == 0; }
            nu_buf_t *native() { return &buf; }
        };

    } // namespace buffer

    namespace fiberscheduler {

        using Fiber = nu_fiber_t;
        using State = nu_fiber_state_t;
        using Scheduler = nu_scheduler_t;

        constexpr State READY = NU_FIBER_READY;
        constexpr State RUNNING = NU_FIBER_RUNNING;
        constexpr State DEAD = NU_FIBER_DEAD;

        inline void initialize(memory::Manager &mm) {
            nu_sched_init(mm.native());
        }

        inline void create(memory::Manager &mm, void (*entry)(void *), void *arg = nullptr) {
            nu_fiber_create(mm.native(), entry, arg);
        }

        inline void yield() {
            nu_yield();
        }

        inline void run() {
            nu_sched_run();
        }

    } // namespace fiberscheduler

    namespace lexer {

        using Token = nu_token_t;
        using Lexer = nu_lexer_t;

        inline void initialize(Lexer &lexer, const char *source) {
            nu_lexer_init(&lexer, source);
        }

        inline char peek(Lexer &lexer) {
            return nu_lexer_peek_char(&lexer);
        }

        inline char advance(Lexer &lexer) {
            return nu_lexer_advance_char(&lexer);
        }

        inline void skip_whitespace(Lexer &lexer) {
            nu_lexer_skip_whitespace(&lexer);
        }

    } // namespace lexer

    namespace parser {

        using Parser = nu_parser_t;
        using Rule = nu_parser_rule_t;
        using Nud = nu_nud_fn;
        using Led = nu_led_fn;

        inline void initialize(Parser &parser, lexer::Lexer &lexer, ast::AST &ast, nu_token_t (*next_token)(nu_lexer_t *), const Rule *rules, uint32_t max_token_id) {
            nu_parser_init(&parser, &lexer, ast.native(), next_token, rules, max_token_id);
        }

        inline void advance(Parser &parser) {
            nu_parser_advance(&parser);
        }

        inline bool match(Parser &parser, uint32_t expected) {
            return nu_parser_match(&parser, expected);
        }

        inline ast::Node *expression(Parser &parser, uint32_t binding_power = 0) {
            return nu_parse_expression(&parser, binding_power);
        }

    } // namespace parser

    namespace args {

        using Definition = nu_arg_def_t;
        using Type = nu_arg_type_t;
        using Parser = nu_arg_parser_t;

        constexpr Type BOOL = NU_ARG_BOOL;
        constexpr Type INT = NU_ARG_INT;
        constexpr Type STRING = NU_ARG_STR;

        class Arguments {
        private:
            nu_arg_parser_t *handle = nullptr;

        public:
            explicit Arguments(memory::Manager &mm) : handle(nu_arg_create(mm.native())) {}
            ~Arguments() { if (handle) nu_arg_destroy(handle); }

            Arguments(const Arguments &) = delete;
            Arguments &operator=(const Arguments &) = delete;

            bool register_argument(Definition &definition) {
                return handle && nu_arg_register(handle, &definition);
            }

            bool parse(int argc, char *argv[]) {
                return handle && nu_arg_parse(handle, argc, argv);
            }

            void help(Definition *definitions, size_t count) {
                if (handle) nu_arg_print_help(handle, definitions, count);
            }

            const char *program_name() const {
                return handle ? handle->program_name : nullptr;
            }

            size_t positional_count() const {
                return handle ? handle->positional_count : 0;
            }

            const char *positional(size_t index) const {
                return handle && index < handle->positional_count ? handle->positionals[index] : nullptr;
            }

            Parser *native() const { return handle; }
        };

    } // namespace args

    namespace event {

        using Loop = nu_loop_t;
        using Callback = nu_event_cb;

        class EventLoop {
        private:
            nu_loop_t *handle = nullptr;
            nu_mm_t *mm = nullptr;

        public:
            explicit EventLoop(memory::Manager &manager) : mm(manager.native()), handle(nu_loop_create(mm)) {}
            ~EventLoop() { if (handle) nu_loop_destroy(handle, mm); }

            EventLoop(const EventLoop &) = delete;
            EventLoop &operator=(const EventLoop &) = delete;

            bool add_fd(int fd, Callback callback, void *data = nullptr) {
                return handle && nu_loop_add_fd(handle, mm, fd, callback, data);
            }

            bool add_timer(int milliseconds, Callback callback, void *data = nullptr) {
                return handle && nu_loop_add_timer(handle, mm, milliseconds, callback, data);
            }

            bool add_watch(const char *path, Callback callback, void *data = nullptr) {
                return handle && nu_loop_add_watch(handle, mm, path, callback, data);
            }

            bool add_signal(int signal, Callback callback, void *data = nullptr) {
                return handle && nu_loop_add_signal(handle, mm, signal, callback, data);
            }

            bool run() {
                return handle && nu_loop_run(handle);
            }

            nu_loop_t *native() const { return handle; }
        };

    } // namespace event

    namespace ipc {

        using Callback = nu_ipc_cb;

        inline void listen(memory::Manager &mm, const char *socket_path, Callback callback, void *data = nullptr) {
            nu_ipc_listen(mm.native(), socket_path, callback, data);
        }

        inline char *send(memory::Manager &mm, const char *socket_path, const char *message) {
            return nu_ipc_send(mm.native(), socket_path, message);
        }

        inline char *send(memory::Manager &mm, const std::string &socket_path, const std::string &message) {
            return send(mm, socket_path.c_str(), message.c_str());
        }

    } // namespace ipc

    namespace process {

        using IOCallback = nu_proc_io_cb;

        inline bool spawn(event::EventLoop &loop, memory::Manager &mm, char *const argv[], IOCallback callback = nullptr, void *data = nullptr) {
            return nu_process_spawn(loop.native(), mm.native(), argv, callback, data);
        }

    } // namespace process

    namespace lua {

        using Context = nu_lua_t;
        using Function = nu_lua_fn;

        class Runtime {
        private:
            nu_lua_t *handle = nullptr;

        public:
            Runtime(memory::Manager &mm, event::EventLoop &loop) : handle(nu_lua_create(mm.native(), loop.native())) {}
            ~Runtime() { if (handle) nu_lua_destroy(handle); }

            Runtime(const Runtime &) = delete;
            Runtime &operator=(const Runtime &) = delete;

            bool run(const char *script) { return handle && nu_lua_run_string(handle, script); }
            bool run(const std::string &script) { return run(script.c_str()); }

            bool run_encrypted(const uint8_t *data, size_t size, const uint32_t key[4]) {
                return handle && nu_lua_run_encrypted(handle, data, size, key);
            }

            void register_function(const char *name, Function fn) {
                if (handle) nu_lua_register(handle, name, fn);
            }

            int top() const { return handle ? nu_lua_get_top(handle) : 0; }
            int64_t to_int(int index) const { return handle ? nu_lua_to_int(handle, index) : 0; }
            double to_float(int index) const { return handle ? nu_lua_to_float(handle, index) : 0.0; }
            const char *to_string(int index, size_t *length = nullptr) const { return handle ? nu_lua_to_str(handle, index, length) : nullptr; }
            bool to_bool(int index) const { return handle && nu_lua_to_bool(handle, index); }

            void push(int64_t value) { if (handle) nu_lua_push_int(handle, value); }
            void push(double value) { if (handle) nu_lua_push_float(handle, value); }
            void push(const char *value) { if (handle) nu_lua_push_str(handle, value); }
            void push(bool value) { if (handle) nu_lua_push_bool(handle, value); }

            Context *native() const { return handle; }
        };

    } // namespace lua

    namespace python {

        using Context = nu_py_t;
        using Function = nu_py_fn;

        class Runtime {
        private:
            nu_py_t *handle = nullptr;

        public:
            Runtime(memory::Manager &mm, event::EventLoop &loop) : handle(nu_py_create(mm.native(), loop.native())) {}
            ~Runtime() { if (handle) nu_py_destroy(handle); }

            Runtime(const Runtime &) = delete;
            Runtime &operator=(const Runtime &) = delete;

            bool run(const char *script) { return handle && nu_py_run_string(handle, script); }
            bool run(const std::string &script) { return run(script.c_str()); }

            void register_function(const char *name, Function fn) {
                if (handle) nu_py_register(handle, name, fn);
            }

            int argc() const { return handle ? nu_py_get_argc(handle) : 0; }
            int64_t to_int(int index) const { return handle ? nu_py_to_int(handle, index) : 0; }
            double to_float(int index) const { return handle ? nu_py_to_float(handle, index) : 0.0; }
            const char *to_string(int index) const { return handle ? nu_py_to_str(handle, index) : nullptr; }
            bool to_bool(int index) const { return handle && nu_py_to_bool(handle, index); }

            void push(int64_t value) { if (handle) nu_py_push_int(handle, value); }
            void push(double value) { if (handle) nu_py_push_float(handle, value); }
            void push(const char *value) { if (handle) nu_py_push_str(handle, value); }
            void push(bool value) { if (handle) nu_py_push_bool(handle, value); }

            Context *native() const { return handle; }
        };

    } // namespace python

    namespace utils {

        using LogLevel = nu_log_level_t;

        constexpr LogLevel DEBUG = NU_LOG_DEBUG;
        constexpr LogLevel INFO = NU_LOG_INFO;
        constexpr LogLevel WARN = NU_LOG_WARN;
        constexpr LogLevel ERROR = NU_LOG_ERROR;

        inline int vsnprintf(char *dest, size_t size, const char *format, va_list args) {
            return nu_vsnprintf(dest, size, format, args);
        }

        inline int snprintf(char *dest, size_t size, const char *format, ...) {
            va_list args;
            va_start(args, format);
            int result = nu_vsnprintf(dest, size, format, args);
            va_end(args);
            return result;
        }

        inline int printf(const char *format, ...) {
            char buffer[4096];
            va_list args;
            va_start(args, format);
            int length = nu_vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            if (length <= 0) return length;
            size_t size = static_cast<size_t>(length);
            if (size >= sizeof(buffer)) size = sizeof(buffer) - 1;
            return nu_fd_write(1, buffer, size);
        }

        inline int write(int fd, const char *data, size_t length) {
            return nu_fd_write(fd, data, length);
        }

        inline int write(int fd, const std::string &data) {
            return write(fd, data.data(), data.size());
        }

        template <typename... Args>
        inline void log(LogLevel level, const char *file, int line, const char *format, Args &&...args) {
            if (!format) return;
            char message[4096]{};
            if constexpr (sizeof...(Args) == 0) {
                std::snprintf(message, sizeof(message), "%s", format);
            } else {
                std::snprintf(message, sizeof(message), format, std::forward<Args>(args)...);
            }
            nu_log_output(level, file ? file : "<unknown>", line, "%s", message);
        }

    } // namespace utils

    namespace std {

        inline char *strdup(const char *str) { return nu_strdup(str); }
        inline size_t strlen(const char *str) { return nu_strlen(str); }
        inline char *strlcpy(char *dest, const char *src, size_t size) { return nu_strlcpy(dest, src, size); }
        inline size_t strnlen(const char *str, size_t maxlen) { return nu_strnlen(str, maxlen); }

        inline int strcmp(const char *a, const char *b) { return nu_strcmp(a, b); }
        inline int strncmp(const char *a, const char *b, size_t size) { return nu_strncmp(a, b, size); }

        inline char *strcpy(char *dest, const char *src) { return nu_strcpy(dest, src); }
        inline char *strncpy(char *dest, const char *src, size_t size) { return nu_strncpy(dest, src, size); }
        inline char *strcat(char *dest, const char *src) { return nu_strcat(dest, src); }
        inline char *strncat(char *dest, const char *src, size_t size) { return nu_strncat(dest, src, size); }

        inline char *strchr(const char *str, int c) { return nu_strchr(str, c); }
        inline char *strrchr(const char *str, int c) { return nu_strrchr(str, c); }
        inline char *strstr(const char *haystack, const char *needle) { return nu_strstr(haystack, needle); }

        inline int atoi(const char *str) { return nu_atoi(str); }
        inline long strtol(const char *str, char **end, int base) { return nu_strtol(str, end, base); }
        inline int abs(int value) { return nu_abs(value); }
        inline int rand() { return nu_rand(); }
        inline void srand(unsigned int seed) { nu_srand(seed); }

        inline int isdigit(int c) { return nu_isdigit(c); }
        inline int isalpha(int c) { return nu_isalpha(c); }
        inline int isalnum(int c) { return nu_isalnum(c); }
        inline int isspace(int c) { return nu_isspace(c); }
        inline int isupper(int c) { return nu_isupper(c); }
        inline int islower(int c) { return nu_islower(c); }
        inline int toupper(int c) { return nu_toupper(c); }
        inline int tolower(int c) { return nu_tolower(c); }

    } // namespace std

} // namespace nu

#define LUA_IMPL
#include <third-party/minilua.h>
#include <nu.h>

struct nu_lua {
    lua_State *L;
    nu_mm_t *mm;
    nu_loop_t *loop;
    nu_lua_fn current_native_callback; // Simplifies context wrapping
};

static void* nu_internal_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    nu_mm_t *mm = (nu_mm_t *)ud;
    if (nsize == 0) {
        if (ptr) nu_free(mm, ptr);
        return NULL;
    }
    void *new_ptr = nu_alloc(mm, nsize);
    if (!new_ptr) return NULL;
    if (ptr) {
        size_t copy_sz = (osize < nsize) ? osize : nsize;
        __builtin_memcpy(new_ptr, ptr, copy_sz);
        nu_free(mm, ptr);
    }
    return new_ptr;
}

nu_lua_t* nu_lua_create(nu_mm_t *mm, nu_loop_t *loop) {
    nu_lua_t *ctx = (nu_lua_t *)nu_alloc(mm, sizeof(nu_lua_t));
    if (!ctx) return NULL;

    ctx->mm = mm;
    ctx->loop = loop;
    ctx->L = lua_newstate(nu_internal_lua_alloc, mm, 0x1337);
    if (!ctx->L) {
        nu_free(mm, ctx);
        return NULL;
    }

    luaL_openlibs(ctx->L);
    return ctx;
}

bool nu_lua_run_string(nu_lua_t *ctx, const char *script) {
    if (luaL_loadstring(ctx->L, script) != 0) return false;
    return (lua_pcall(ctx->L, 0, 0, 0) == 0);
}

bool nu_lua_run_encrypted(nu_lua_t *ctx, const uint8_t *enc_data, size_t size, const uint32_t key[4]) {
    uint8_t *dec_buf = (uint8_t *)nu_alloc(ctx->mm, size);
    if (!dec_buf) return false;

    __builtin_memcpy(dec_buf, enc_data, size);
    nu_xtea_crypt_ctr(key, 0, dec_buf, size); // CTR streaming decryption

    int status = luaL_loadbuffer(ctx->L, (const char *)dec_buf, size, "=(secure_blob)");
    nu_free(ctx->mm, dec_buf);

    if (status != 0) return false;
    return (lua_pcall(ctx->L, 0, 0, 0) == 0);
}

static int nu_internal_lua_thunk(lua_State *L) {
    nu_lua_t *ctx = (nu_lua_t *)lua_touserdata(L, lua_upvalueindex(1));
    nu_lua_fn real_fn = (nu_lua_fn)lua_touserdata(L, lua_upvalueindex(2));
    return real_fn(ctx);
}

void nu_lua_register(nu_lua_t *ctx, const char *name, nu_lua_fn fn) {
    lua_pushlightuserdata(ctx->L, ctx);
    lua_pushlightuserdata(ctx->L, (void*)fn);
    lua_pushcclosure(ctx->L, nu_internal_lua_thunk, 2);
    lua_setglobal(ctx->L, name);
}

void nu_lua_destroy(nu_lua_t *ctx) {
    if (!ctx) return;
    nu_mm_t *mm = ctx->mm;
    lua_close(ctx->L);
    nu_free(mm, ctx);
}

/* Opaque stack mappings */
int         nu_lua_get_top(nu_lua_t *ctx)                     { return lua_gettop(ctx->L); }
int64_t     nu_lua_to_int(nu_lua_t *ctx, int index)           { return (int64_t)lua_tointeger(ctx->L, index); }
double      nu_lua_to_float(nu_lua_t *ctx, int index)         { return (double)lua_tonumber(ctx->L, index); }
const char* nu_lua_to_str(nu_lua_t *ctx, int index, size_t *l){ return lua_tolstring(ctx->L, index, l); }
bool        nu_lua_to_bool(nu_lua_t *ctx, int index)          { return (bool)lua_toboolean(ctx->L, index); }

void        nu_lua_push_int(nu_lua_t *ctx, int64_t val)       { lua_pushinteger(ctx->L, val); }
void        nu_lua_push_float(nu_lua_t *ctx, double val)     { lua_pushnumber(ctx->L, val); }
void        nu_lua_push_str(nu_lua_t *ctx, const char *str)   { lua_pushstring(ctx->L, str); }
void        nu_lua_push_bool(nu_lua_t *ctx, bool val)         { lua_pushboolean(ctx->L, val); }

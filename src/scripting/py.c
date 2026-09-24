#include <stdlib.h>
#include <string.h>
#include <nu.h>

static _Thread_local nu_mm_t *current_py_mm = NULL;
#define malloc(sz)      nu_alloc(current_py_mm, sz)
#define free(ptr)       nu_free(current_py_mm, ptr)
#define realloc(p, sz)  nu_realloc(current_py_mm, p, sz)

#include <third-party/pocketpy.h>

struct nu_py {
    nu_mm_t *mm;
    nu_loop_t *loop;
    int argc;
    py_Ref argv;
    py_Ref retval;
};

#define NU_MAX_PY_CALLBACKS 64
static struct {
    const char *name;
    nu_py_fn native_handler;
} nu_py_registry[NU_MAX_PY_CALLBACKS];
static int nu_py_registry_count = 0;

static _Thread_local nu_py_t *active_py_ctx = NULL;

#define GENERATE_BRIDGE(index) \
    static bool nu_py_bridge_##index(int argc, py_Ref argv) { \
        if (!active_py_ctx) return false; \
        active_py_ctx->argc = argc; \
        active_py_ctx->argv = argv; \
        active_py_ctx->retval = py_retval(); \
        nu_py_registry[index].native_handler(active_py_ctx); \
        return true; \
    }

GENERATE_BRIDGE(0)  GENERATE_BRIDGE(1)  GENERATE_BRIDGE(2)  GENERATE_BRIDGE(3)
GENERATE_BRIDGE(4)  GENERATE_BRIDGE(5)  GENERATE_BRIDGE(6)  GENERATE_BRIDGE(7)
GENERATE_BRIDGE(8)  GENERATE_BRIDGE(9)  GENERATE_BRIDGE(10) GENERATE_BRIDGE(11)
GENERATE_BRIDGE(12) GENERATE_BRIDGE(13) GENERATE_BRIDGE(14) GENERATE_BRIDGE(15)
GENERATE_BRIDGE(16) GENERATE_BRIDGE(17) GENERATE_BRIDGE(18) GENERATE_BRIDGE(19)
GENERATE_BRIDGE(20) GENERATE_BRIDGE(21) GENERATE_BRIDGE(22) GENERATE_BRIDGE(23)
GENERATE_BRIDGE(24) GENERATE_BRIDGE(25) GENERATE_BRIDGE(26) GENERATE_BRIDGE(27)
GENERATE_BRIDGE(28) GENERATE_BRIDGE(29) GENERATE_BRIDGE(30) GENERATE_BRIDGE(31)
GENERATE_BRIDGE(32) GENERATE_BRIDGE(33) GENERATE_BRIDGE(34) GENERATE_BRIDGE(35)
GENERATE_BRIDGE(36) GENERATE_BRIDGE(37) GENERATE_BRIDGE(38) GENERATE_BRIDGE(39)
GENERATE_BRIDGE(40) GENERATE_BRIDGE(41) GENERATE_BRIDGE(42) GENERATE_BRIDGE(43)
GENERATE_BRIDGE(44) GENERATE_BRIDGE(45) GENERATE_BRIDGE(46) GENERATE_BRIDGE(47)
GENERATE_BRIDGE(48) GENERATE_BRIDGE(49) GENERATE_BRIDGE(50) GENERATE_BRIDGE(51)
GENERATE_BRIDGE(52) GENERATE_BRIDGE(53) GENERATE_BRIDGE(54) GENERATE_BRIDGE(55)
GENERATE_BRIDGE(56) GENERATE_BRIDGE(57) GENERATE_BRIDGE(58) GENERATE_BRIDGE(59)
GENERATE_BRIDGE(60) GENERATE_BRIDGE(61) GENERATE_BRIDGE(62) GENERATE_BRIDGE(63)

static const py_CFunction nu_bridge_table[NU_MAX_PY_CALLBACKS] = {
    nu_py_bridge_0,  nu_py_bridge_1,  nu_py_bridge_2,  nu_py_bridge_3,
    nu_py_bridge_4,  nu_py_bridge_5,  nu_py_bridge_6,  nu_py_bridge_7,
    nu_py_bridge_8,  nu_py_bridge_9,  nu_py_bridge_10, nu_py_bridge_11,
    nu_py_bridge_12, nu_py_bridge_13, nu_py_bridge_14, nu_py_bridge_15,
    nu_py_bridge_16, nu_py_bridge_17, nu_py_bridge_18, nu_py_bridge_19,
    nu_py_bridge_20, nu_py_bridge_21, nu_py_bridge_22, nu_py_bridge_23,
    nu_py_bridge_24, nu_py_bridge_25, nu_py_bridge_26, nu_py_bridge_27,
    nu_py_bridge_28, nu_py_bridge_29, nu_py_bridge_30, nu_py_bridge_31,
    nu_py_bridge_32, nu_py_bridge_33, nu_py_bridge_34, nu_py_bridge_35,
    nu_py_bridge_36, nu_py_bridge_37, nu_py_bridge_38, nu_py_bridge_39,
    nu_py_bridge_40, nu_py_bridge_41, nu_py_bridge_42, nu_py_bridge_43,
    nu_py_bridge_44, nu_py_bridge_45, nu_py_bridge_46, nu_py_bridge_47,
    nu_py_bridge_48, nu_py_bridge_49, nu_py_bridge_50, nu_py_bridge_51,
    nu_py_bridge_52, nu_py_bridge_53, nu_py_bridge_54, nu_py_bridge_55,
    nu_py_bridge_56, nu_py_bridge_57, nu_py_bridge_58, nu_py_bridge_59,
    nu_py_bridge_60, nu_py_bridge_61, nu_py_bridge_62, nu_py_bridge_63
};

nu_py_t* nu_py_create(nu_mm_t *mm, nu_loop_t *loop) {
    current_py_mm = mm;
    nu_py_t *ctx = (nu_py_t *)nu_alloc(mm, sizeof(nu_py_t));
    if (!ctx) return NULL;
    ctx->mm = mm;
    ctx->loop = loop;
    ctx->argc = 0;
    ctx->argv = NULL;
    ctx->retval = NULL;
    py_initialize();
    return ctx;
}

bool nu_py_run_string(nu_py_t *ctx, const char *script) {
    if (!ctx) return false;
    current_py_mm = ctx->mm;
    active_py_ctx = ctx;
    bool status = py_exec(script, "<string>", EXEC_MODE, NULL);
    active_py_ctx = NULL;
    return status;
}

void nu_py_register(nu_py_t *ctx, const char *name, nu_py_fn fn) {
    if (!ctx || nu_py_registry_count >= NU_MAX_PY_CALLBACKS) return;
    current_py_mm = ctx->mm;
    int idx = nu_py_registry_count;
    nu_py_registry[idx].name = name;
    nu_py_registry[idx].native_handler = fn;
    py_Ref r0 = py_tmpr0();
    py_newnativefunc(r0, nu_bridge_table[idx]);
    py_setglobal(py_name(name), r0);
    nu_py_registry_count++;
}

int nu_py_get_argc(nu_py_t *ctx) {
    return ctx->argc;
}

int64_t nu_py_to_int(nu_py_t *ctx, int index) {
    if (index >= ctx->argc) return 0;
    py_Ref argv = ctx->argv;
    return (int64_t)py_toint(py_arg(index));
}

double nu_py_to_float(nu_py_t *ctx, int index) {
    if (index >= ctx->argc) return 0.0;
    py_Ref argv = ctx->argv;
    return (double)py_tofloat(py_arg(index));
}

const char* nu_py_to_str(nu_py_t *ctx, int index) {
    if (index >= ctx->argc) return NULL;
    py_Ref argv = ctx->argv;
    return py_tostr(py_arg(index));
}

bool nu_py_to_bool(nu_py_t *ctx, int index) {
    if (index >= ctx->argc) return false;
    py_Ref argv = ctx->argv;
    return py_tobool(py_arg(index));
}

void nu_py_push_int(nu_py_t *ctx, int64_t val) {
    if (ctx->retval) {
        py_newint(ctx->retval, (py_i64)val);
    }
}

void nu_py_push_float(nu_py_t *ctx, double val) {
    if (ctx->retval) {
        py_newfloat(ctx->retval, val);
    }
}

void nu_py_push_str(nu_py_t *ctx, const char *str) {
    if (ctx->retval) {
        py_newstr(ctx->retval, str);
    }
}

void nu_py_push_bool(nu_py_t *ctx, bool val) {
    if (ctx->retval) {
        py_newbool(ctx->retval, val);
    }
}

void nu_py_destroy(nu_py_t *ctx) {
    if (!ctx) return;
    nu_mm_t *mm = ctx->mm;
    current_py_mm = mm;
    py_finalize();
    nu_free(mm, ctx);
    current_py_mm = NULL;
}


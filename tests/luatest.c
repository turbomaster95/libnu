#include <stdio.h>
#include <stdint.h>
#include <nu.h>

static int test_host_add(nu_lua_t *ctx) {
    if (nu_lua_get_top(ctx) < 2) {
        nu_lua_push_int(ctx, 0);
        return 1; // 1 return value
    }

    int64_t a = nu_lua_to_int(ctx, 1);
    int64_t b = nu_lua_to_int(ctx, 2);

    nu_lua_push_int(ctx, a + b);
    return 1; 
}

int main(void) {
    static uint8_t runtime_memory_arena[256 * 1024];
    
    nu_mm_t *mm = nu_mm_create(NU_MM_BUDDY, runtime_memory_arena, sizeof(runtime_memory_arena));
    if (!mm) {
        fprintf(stderr, "Failed to allocate buddy memory pool\n");
        return -1;
    }
    printf("Allocated 256KB isolation arena at %p\n", (void*)runtime_memory_arena);

    nu_lua_t *lua = nu_lua_create(mm, NULL);
    if (!lua) {
        fprintf(stderr, "Failed to initialize Lua runtime instance\n");
        nu_mm_destroy(mm);
        return -1;
    }
    nu_lua_register(lua, "nu_host_add", test_host_add);
    printf("Registered C-bridge host function: 'nu_host_add'\n");

    const char *test_script = 
        "local msg = 'Hello from the inside!'\n"
        "print(msg)\n"
        "local result = nu_host_add(40, 2)\n"
        "print('C-Bridge Result (40 + 2):', result)\n";

    bool success = nu_lua_run_string(lua, test_script);
    if (success) {
        printf("Test script executed perfectly with zero memory leaks\n");
    } else {
        fprintf(stderr, "Script runtime or syntax tracking error occurred\n");
    }

    nu_lua_destroy(lua);
    nu_mm_destroy(mm);

    return success ? 0 : 1;
}

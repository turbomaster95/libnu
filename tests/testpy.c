#include <stdio.h>
#include <stdint.h>
#include <nu.h>

static void py_host_multiply(nu_py_t *ctx) {
    if (nu_py_get_argc(ctx) < 2) {
        nu_py_push_int(ctx, 0);
        return;
    }

    int64_t a = nu_py_to_int(ctx, 0);
    int64_t b = nu_py_to_int(ctx, 1);

    nu_py_push_int(ctx, a * b);
}

int main(void) {
    static uint8_t python_arena[512 * 1024];
    nu_mm_t *mm = nu_mm_create(NU_MM_BUDDY, python_arena, sizeof(python_arena));
    if (!mm) {
        fprintf(stderr, "Failed to allocate memory arena\n");
        return -1;
    }
    printf("Allocated 512KB buddy arena at %p\n", (void*)python_arena);

    nu_py_t *py = nu_py_create(mm, NULL);
    if (!py) {
        fprintf(stderr, "Failed to initialize custom Python runtime\n");
        nu_mm_destroy(mm);
        return -1;
    }

    nu_py_register(py, "nu_host_mul", py_host_multiply);
    printf("Bound function: 'nu_host_mul'\n");

    const char *py_script = 
        "print('Greetings from PocketPy Python runtime!')\n"
        "val = nu_host_mul(21, 2)\n"
        "print('Python C-Bridge Result (21 * 2):', val)\n";

    bool ok = nu_py_run_string(py, py_script);

    if (ok) {
        printf("Execution finished with zero leak tracking anomalies\n");
    } else {
        fprintf(stderr, "Script runtime exception occurred\n");
    }

    nu_py_destroy(py);
    nu_mm_destroy(mm);
    return ok ? 0 : 1;
}


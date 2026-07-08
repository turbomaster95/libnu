#ifndef NUO_H
#define NUO_H

#define NU_SMASH_CFG() do {                                                      \
    __label__ nu_local_lbl;                                                      \
    __asm__ goto("jmp %l[nu_local_lbl]" : : : : nu_local_lbl);                   \
    nu_local_lbl:;                                                               \
} while(0)

#define NU_SMASH_DATA(var) __asm__ volatile("" : "+g"(var))

#define NU_SMASH_FLAGS() __asm__ volatile("" : : : "cc")

#define NU_SMASH_REGS() __asm__ volatile("" : : : "memory",                     \
                              "rax", "rcx", "rdx", "rsi", "rdi",                 \
                              "r8", "r9", "r10", "r11")

#define NU_SMASH_SIMD() __asm__ volatile("" : : : "xmm0", "xmm1", "xmm2", "xmm3",\
                                                   "xmm4", "xmm5", "xmm6", "xmm7")
#define NU_OBFS_CHOOSER(_0, _1, NAME, ...) NAME
#define nu_obfs(...) NU_OBFS_CHOOSER(0, ##__VA_ARGS__, nu_obfs_scaled, nu_obfs_default)(__VA_ARGS__)

#define nu_obfs_default() nu_obfs_scaled(1)

#define nu_obfs_scaled(intensity) do {                                           \
    NU_SMASH_REGS();                                                             \
    NU_SMASH_SIMD();                                                             \
    NU_SMASH_FLAGS();                                                            \
                                                                                 \
    volatile int obfs_loop = (intensity);                                        \
    while (obfs_loop-- > 0) {                                                    \
        NU_SMASH_DATA(obfs_loop);      /* Blind loop tracker */                  \
        NU_SMASH_CLEAN_CFG();          /* Insert basic-block chaos */            \
                                                                                 \
        volatile int obfs_zero = 0;                                              \
        NU_SMASH_DATA(obfs_zero);      /* Blind the predicate value */           \
                                                                                 \
        if (__builtin_expect(obfs_zero, 0)) {                                    \
            __asm__ volatile(                                                    \
                "nop\n\t"                                                        \
                "xor %%rax, %%rax\n\t"                                           \
                "sub $0x40, %%rsp\n\t"                                           \
                "add $0x40, %%rsp\n\t"                                           \
                : : : "memory"                                                   \
            );                                                                   \
        }                                                                        \
    }                                                                            \
    __asm__ volatile("" : : : "memory");                                         \
} while(0)

// Helper macro to ensure internal calls don't collide with exterior macro scopes
#define NU_SMASH_CLEAN_CFG() do {                                                \
    __label__ nu_int_lbl;                                                        \
    __asm__ goto("jmp %l[nu_int_lbl]" : : : : nu_int_lbl);                       \
    nu_int_lbl:;                                                                 \
} while(0)

#endif // NUO_H

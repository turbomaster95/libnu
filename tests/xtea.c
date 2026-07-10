#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <nu.h>
#include <nuo.h>

__attribute__((noinline))
void xtea_encrypt_block(uint32_t num_rounds, uint32_t v[2], uint32_t const key[4]) {
    uint32_t i;
    uint32_t v0 = v[0], v1 = v[1], sum = 0;
    uint32_t delta = 0x9E3779B9;

    NU_SMASH_DATA(v0);
    NU_SMASH_DATA(v1);
    NU_SMASH_DATA(sum);

    for (i = 0; i < num_rounds; i++) {
        NU_SMASH_SIMD();
        nu_obfs(4); 

        NU_SMASH_DATA(v1);
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        
        NU_SMASH_FLAGS();
        sum += delta;
        
        NU_SMASH_DATA(v0);
        NU_SMASH_DATA(sum);
        
        NU_SMASH_SPECIFIC_CFG: 
        NU_SMASH_CFG();

        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
    }

    nu_obfs(8);
    v[0] = v0; v[1] = v1;
}

__attribute__((noinline))
void xtea_decrypt_block(uint32_t num_rounds, uint32_t v[2], uint32_t const key[4]) {
    uint32_t i;
    uint32_t v0 = v[0], v1 = v[1];
    uint32_t delta = 0x9E3779B9;
    uint32_t sum = delta * num_rounds;

    NU_SMASH_DATA(v0);
    NU_SMASH_DATA(v1);
    NU_SMASH_DATA(sum);

    for (i = 0; i < num_rounds; i++) {
        NU_SMASH_SIMD();
        nu_obfs(4);

        NU_SMASH_DATA(v0);
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
        
        NU_SMASH_FLAGS();
        sum -= delta;
        
        NU_SMASH_DATA(v1);
        NU_SMASH_DATA(sum);
        
        NU_SMASH_REG_JUMP:
        NU_SMASH_ID_CFG:
        NU_SMASH_CFG();

        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }

    nu_obfs(8);
    v[0] = v0; v[1] = v1;
}

int main(void) {
    uint32_t key[4] = {0xDEADBEEF, 0xCAFEBABE, 0x01234567, 0x89ABCDEF};
    char asset[8] = {'M', 'o', 'l', 't', '_', 'O', 'S', '\0'}; 
    uint32_t *block = (uint32_t *)asset;

    printf("Original Asset:  %s\n", asset);

    printf("Encrypting...\n");
    xtea_encrypt_block(32, block, key);
    printf("Encrypted Hex:   0x%08X %08X\n", block[0], block[1]);

    nu_obfs(12);

    printf("Decrypting...\n");
    xtea_decrypt_block(32, block, key);
    printf("Decrypted Asset: %s\n", asset);

    return 0;
}

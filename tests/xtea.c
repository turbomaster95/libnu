#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <nu.h>

int main(void) {
    char message[] = "This is an odd-sized secret msg!"; /* 32 bytes */
    size_t msg_len = strlen(message);
    uint64_t simple_nonce = 0xABCDEF1234567890ULL;

    uint32_t derived_key[4];
    nu_xtea_derive_key("my_ultra_secure_password_123", derived_key);

    /* Encrypt in place */
    nu_xtea_crypt_ctr(derived_key, simple_nonce, (uint8_t *)message, msg_len);
    assert(strcmp(message, "This is an odd-sized secret msg!") != 0); // Must be scrambled

    nu_xtea_crypt_ctr(derived_key, simple_nonce, (uint8_t *)message, msg_len);
    assert(strcmp(message, "This is an odd-sized secret msg!") == 0); // Must be back to normal

    uint32_t hard_key[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
    uint32_t raw_block[2] = {0xDEADBEEF, 0xCAFEBABE};

    /* Directly target low-level blocks */
    nu_xtea_encrypt_block(hard_key, raw_block);
    assert(raw_block[0] != 0xDEADBEEF);

    nu_xtea_decrypt_block(hard_key, raw_block);
    assert(raw_block[0] == 0xDEADBEEF && raw_block[1] == 0xCAFEBABE);
    return 0;
}


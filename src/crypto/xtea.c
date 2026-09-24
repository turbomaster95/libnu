#include <string.h>
#include <stdint.h>
#include <nu.h>

void nu_xtea_encrypt_block(const uint32_t key[4], uint32_t block[2]) {
    uint32_t v0 = block[0];
    uint32_t v1 = block[1];
    uint32_t sum = 0;
    uint32_t delta = 0x9E3779B9;
    for (int i = 0; i < 32; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
    }
    block[0] = v0;
    block[1] = v1;
}

void nu_xtea_decrypt_block(const uint32_t key[4], uint32_t block[2]) {
    uint32_t v0 = block[0];
    uint32_t v1 = block[1];
    uint32_t delta = 0x9E3779B9;
    uint32_t sum = delta * 32;
    for (int i = 0; i < 32; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }
    block[0] = v0;
    block[1] = v1;
}

void nu_xtea_derive_key(const char *passphrase, uint32_t key[4]) {
    key[0] = 0x12345678; key[1] = 0x9ABCDEF0;
    key[2] = 0xFEB98765; key[3] = 0x43210FED;

    if (!passphrase) return;

    size_t i = 0;
    while (passphrase[i]) {
        /* Avalanche the string bytes cleanly across the key slots */
        key[i % 4] ^= (unsigned char)passphrase[i];
        key[i % 4] *= 16777619u; /* FNV prime step */
        i++;
    }
}

void nu_xtea_crypt_ctr(const uint32_t key[4], uint64_t nonce, uint8_t *data, size_t size) {
    uint64_t counter = 0;
    size_t offset = 0;

    while (offset < size) {
        uint32_t block[2];
        uint64_t keystream_input = nonce ^ counter;
        memcpy(block, &keystream_input, 8);

        nu_xtea_encrypt_block(key, block);

        uint8_t keystream_bytes[8];
        memcpy(keystream_bytes, block, 8);

        size_t chunk = (size - offset < 8) ? (size - offset) : 8;
        for (size_t i = 0; i < chunk; i++) {
            data[offset + i] ^= keystream_bytes[i];
        }

        offset += chunk;
        counter++;
    }
}


#include <nu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !NU_HAS_NATIVE_U128
static nu_u128 soft_add128(nu_u128 a, nu_u128 b) {
    nu_u128 res;
    res.low = a.low + b.low;
    uint64_t carry = (res.low < a.low);
    res.high = a.high + b.high + carry;
    return res;
}

static nu_u128 soft_sub128(nu_u128 a, nu_u128 b) {
    nu_u128 res;
    res.low = a.low - b.low;
    uint64_t borrow = (a.low < b.low);
    res.high = a.high - b.high - borrow;
    return res;
}
#endif

#if !NU_HAS_NATIVE_U256
static nu_u256 to_soft256(nu_calc_t src) {
    nu_u256 res = {{0, 0, 0, 0}};
    if (src.type == NU_PREC_64) {
        res.limbs[0] = src.data.v64;
    } else if (src.type == NU_PREC_128) {
        #if NU_HAS_NATIVE_U128
            res.limbs[0] = (uint64_t)src.data.v128;
            res.limbs[1] = (uint64_t)(src.data.v128 >> 64);
        #else
            res.limbs[0] = src.data.v128.low;
            res.limbs[1] = src.data.v128.high;
        #endif
    } else {
        res = src.data.v256;
    }
    return res;
}

static nu_u256 soft_add256(nu_u256 a, nu_u256 b) {
    nu_u256 res;
    uint64_t carry = 0;
    for (int i = 0; i < 4; i++) {
        uint64_t sum = a.limbs[i] + b.limbs[i] + carry;
        carry = (sum < a.limbs[i]) || (carry && sum == a.limbs[i]);
        res.limbs[i] = sum;
    }
    return res;
}

static nu_u256 soft_sub256(nu_u256 a, nu_u256 b) {
    nu_u256 res;
    uint64_t borrow = 0;
    for (int i = 0; i < 4; i++) {
        uint64_t diff = a.limbs[i] - b.limbs[i] - borrow;
        if (a.limbs[i] < b.limbs[i] || (borrow && a.limbs[i] == b.limbs[i])) {
            borrow = 1;
        } else {
            borrow = 0;
        }
        res.limbs[i] = diff;
    }
    return res;
}
#endif

nu_calc_t nu_calc_from_u64(uint64_t val) {
    nu_calc_t res;
    res.type = NU_PREC_64;
    res.data.v64 = val;
    return res;
}

nu_calc_t nu_calc_add(nu_calc_t a, nu_calc_t b) {
    nu_calc_t res;
    res.type = (a.type > b.type) ? a.type : b.type;

    if (res.type == NU_PREC_64) {
        res.data.v64 = a.data.v64 + b.data.v64;
    } 
    else if (res.type == NU_PREC_128) {
        #if NU_HAS_NATIVE_U128
            nu_u128 va = (a.type == NU_PREC_128) ? a.data.v128 : (nu_u128)a.data.v64;
            nu_u128 vb = (b.type == NU_PREC_128) ? b.data.v128 : (nu_u128)b.data.v64;
            res.data.v128 = va + vb;
        #else
            nu_u128 va = (a.type == NU_PREC_128) ? a.data.v128 : (nu_u128){a.data.v64, 0};
            nu_u128 vb = (b.type == NU_PREC_128) ? b.data.v128 : (nu_u128){b.data.v64, 0};
            res.data.v128 = soft_add128(va, vb);
        #endif
    } 
    else {
        #if NU_HAS_NATIVE_U256
            nu_u256 va = (a.type == NU_PREC_256) ? a.data.v256 :
                         (a.type == NU_PREC_128) ? (nu_u256)a.data.v128 : (nu_u256)a.data.v64;
            nu_u256 vb = (b.type == NU_PREC_256) ? b.data.v256 :
                         (b.type == NU_PREC_128) ? (nu_u256)b.data.v128 : (nu_u256)b.data.v64;
            res.data.v256 = va + vb;
        #else
            nu_u256 va = to_soft256(a);
            nu_u256 vb = to_soft256(b);
            res.data.v256 = soft_add256(va, vb);
        #endif
    }
    return res;
}

nu_calc_t nu_calc_sub(nu_calc_t a, nu_calc_t b) {
    nu_calc_t res;
    res.type = (a.type > b.type) ? a.type : b.type;

    if (res.type == NU_PREC_64) {
        res.data.v64 = a.data.v64 - b.data.v64;
    } 
    else if (res.type == NU_PREC_128) {
        #if NU_HAS_NATIVE_U128
            nu_u128 va = (a.type == NU_PREC_128) ? a.data.v128 : (nu_u128)a.data.v64;
            nu_u128 vb = (b.type == NU_PREC_128) ? b.data.v128 : (nu_u128)b.data.v64;
            res.data.v128 = va - vb;
        #else
            nu_u128 va = (a.type == NU_PREC_128) ? a.data.v128 : (nu_u128){a.data.v64, 0};
            nu_u128 vb = (b.type == NU_PREC_128) ? b.data.v128 : (nu_u128){b.data.v64, 0};
            res.data.v128 = soft_sub128(va, vb);
        #endif
    } 
    else {
        #if NU_HAS_NATIVE_U256
            nu_u256 va = (a.type == NU_PREC_256) ? a.data.v256 :
                         (a.type == NU_PREC_128) ? (nu_u256)a.data.v128 : (nu_u256)a.data.v64;
            nu_u256 vb = (b.type == NU_PREC_256) ? b.data.v256 :
                         (b.type == NU_PREC_128) ? (nu_u256)b.data.v128 : (nu_u256)b.data.v64;
            res.data.v256 = va - vb;
        #else
            nu_u256 va = to_soft256(a);
            nu_u256 vb = to_soft256(b);
            res.data.v256 = soft_sub256(va, vb);
        #endif
    }
    return res;
}

nu_calc_t nu_calc_mul(nu_calc_t a, nu_calc_t b) {
    nu_calc_t res;
    res.type = (a.type > b.type) ? a.type : b.type;

    if (res.type == NU_PREC_64) {
        res.data.v64 = a.data.v64 * b.data.v64;
    } 
    else if (res.type == NU_PREC_128) {
        #if NU_HAS_NATIVE_U128
            nu_u128 va = (a.type == NU_PREC_128) ? a.data.v128 : (nu_u128)a.data.v64;
            nu_u128 vb = (b.type == NU_PREC_128) ? b.data.v128 : (nu_u128)b.data.v64;
            res.data.v128 = va * vb;
        #else
            nu_u128 res128;
            uint64_t a0 = a.data.v128.low, a1 = a.data.v128.high;
            uint64_t b0 = b.data.v128.low, b1 = b.data.v128.high;
            if (a.type == NU_PREC_64) { a0 = a.data.v64; a1 = 0; }
            if (b.type == NU_PREC_64) { b0 = b.data.v64; b1 = 0; }

            res128.low = a0 * b0;
            res128.high = (a1 * b0) + (a0 * b1); 
            res.data.v128 = res128;
        #endif
    } 
    else {
        #if NU_HAS_NATIVE_U256
            nu_u256 va = (a.type == NU_PREC_256) ? a.data.v256 :
                         (a.type == NU_PREC_128) ? (nu_u256)a.data.v128 : (nu_u256)a.data.v64;
            nu_u256 vb = (b.type == NU_PREC_256) ? b.data.v256 :
                         (b.type == NU_PREC_128) ? (nu_u256)b.data.v128 : (nu_u256)b.data.v64;
            res.data.v256 = va * vb;
        #else
            nu_u256 va = to_soft256(a);
            nu_u256 vb = to_soft256(b);
            nu_u256 prod = {{0, 0, 0, 0}};
            
            for (int i = 0; i < 4; i++) {
                uint64_t carry = 0;
                for (int j = 0; j < 4 - i; j++) {
                    #if NU_HAS_NATIVE_U128
                        __uint128_t temp = (__uint128_t)va.limbs[i] * vb.limbs[j] + prod.limbs[i+j] + carry;
                        prod.limbs[i+j] = (uint64_t)temp;
                        carry = (uint64_t)(temp >> 64);
                    #else
                        uint64_t a_low = va.limbs[i] & 0xFFFFFFFF, a_high = va.limbs[i] >> 32;
                        uint64_t b_low = vb.limbs[j] & 0xFFFFFFFF, b_high = vb.limbs[j] >> 32;
                        
                        uint64_t p0 = a_low * b_low;
                        uint64_t p1 = a_low * b_high;
                        uint64_t p2 = a_high * b_low;
                        uint64_t p3 = a_high * b_high;
                        
                        uint64_t w_sum = prod.limbs[i+j] + carry;
                        uint64_t mid = p1 + p2 + (w_sum >> 32) + (p0 >> 32);
                        
                        prod.limbs[i+j] = (mid << 32) | ((p0 + w_sum) & 0xFFFFFFFF);
                        carry = p3 + (mid >> 32);
                    #endif
                }
            }
            res.data.v256 = prod;
        #endif
    }
    return res;
}

void nu_calc_print(nu_calc_t val) {
    if (val.type == NU_PREC_64) {
        printf("%lu (64-bit)\n", val.data.v64);
    } 
    else if (val.type == NU_PREC_128) {
        #if NU_HAS_NATIVE_U128
            uint64_t hi = (uint64_t)(val.data.v128 >> 64);
            uint64_t lo = (uint64_t)val.data.v128;
        #else
            uint64_t hi = val.data.v128.high;
            uint64_t lo = val.data.v128.low;
        #endif
        printf("0x%016lx%016lx (128-bit hex)\n", hi, lo);
    } 
    else {
        printf("0x");
        #if NU_HAS_NATIVE_U256
            for (int i = 3; i >= 0; i--) {
                uint64_t chunk = (uint64_t)(val.data.v256 >> (i * 64));
                printf("%016lx", chunk);
            }
        #else
            for (int i = 3; i >= 0; i--) {
                printf("%016lx", val.data.v256.limbs[i]);
            }
        #endif
        printf(" (256-bit hex)\n");
    }
}

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

static int soft_cmp128(nu_u128 a, nu_u128 b) {
    if (a.high != b.high) return (a.high > b.high) ? 1 : -1;
    if (a.low != b.low) return (a.low > b.low) ? 1 : -1;
    return 0;
}

static nu_u128 soft_mul64(uint64_t a, uint64_t b) {
    nu_u128 res;
    uint64_t a_lo = a & 0xFFFFFFFF, a_hi = a >> 32;
    uint64_t b_lo = b & 0xFFFFFFFF, b_hi = b >> 32;

    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;

    uint64_t mid1 = p1 + (p0 >> 32);
    uint64_t mid2 = mid1 + p2;
    uint64_t carry = (mid1 < (p0 >> 32) ? (1ULL << 32) : 0) + (mid2 < mid1 ? (1ULL << 32) : 0);

    res.low = (mid2 << 32) | (p0 & 0xFFFFFFFF);
    res.high = p3 + (mid2 >> 32) + carry;
    return res;
}

static nu_u128 soft_div128(nu_u128 num, nu_u128 den) {
    nu_u128 quot = {0, 0};
    nu_u128 rem = {0, 0};
    if (den.low == 0 && den.high == 0) return quot;

    for (int i = 127; i >= 0; i--) {
        uint64_t bit = (i >= 64) ? ((num.high >> (i - 64)) & 1) : ((num.low >> i) & 1);
        rem.high = (rem.high << 1) | (rem.low >> 63);
        rem.low = (rem.low << 1) | bit;

        if (soft_cmp128(rem, den) >= 0) {
            rem = soft_sub128(rem, den);
            if (i >= 64) quot.high |= ((uint64_t)1 << (i - 64));
            else quot.low |= ((uint64_t)1 << i);
        }
    }
    return quot;
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
        borrow = (a.limbs[i] < b.limbs[i]) || (borrow && a.limbs[i] == b.limbs[i]);
        res.limbs[i] = diff;
    }
    return res;
}

static int soft_cmp256(nu_u256 a, nu_u256 b) {
    for (int i = 3; i >= 0; i--) {
        if (a.limbs[i] != b.limbs[i]) {
            return (a.limbs[i] > b.limbs[i]) ? 1 : -1;
        }
    }
    return 0;
}

static nu_u256 soft_div256(nu_u256 num, nu_u256 den) {
    nu_u256 quot = {{0, 0, 0, 0}};
    nu_u256 rem = {{0, 0, 0, 0}};
    int is_zero = 1;
    for (int i = 0; i < 4; i++) if (den.limbs[i]) is_zero = 0;
    if (is_zero) return quot;

    for (int i = 255; i >= 0; i--) {
        int limb_idx = i / 64;
        int bit_idx = i % 64;
        uint64_t bit = (num.limbs[limb_idx] >> bit_idx) & 1;

        uint64_t carry = bit;
        for (int j = 0; j < 4; j++) {
            uint64_t next_carry = rem.limbs[j] >> 63;
            rem.limbs[j] = (rem.limbs[j] << 1) | carry;
            carry = next_carry;
        }

        if (soft_cmp256(rem, den) >= 0) {
            rem = soft_sub256(rem, den);
            quot.limbs[limb_idx] |= ((uint64_t)1 << bit_idx);
        }
    }
    return quot;
}
#endif

static nu_u128 to_u128(nu_calc_t src) {
#if NU_HAS_NATIVE_U128
    if (src.type == NU_PREC_64) return (nu_u128)src.data.v64;
    if (src.type == NU_PREC_128) return src.data.v128;
    #if NU_HAS_NATIVE_U256
        return (nu_u128)src.data.v256;
    #else
        return (nu_u128)src.data.v256.limbs[0] | ((nu_u128)src.data.v256.limbs[1] << 64);
    #endif
#else
    if (src.type == NU_PREC_64) return (nu_u128){src.data.v64, 0};
    if (src.type == NU_PREC_128) return src.data.v128;
    return (nu_u128){src.data.v256.limbs[0], src.data.v256.limbs[1]};
#endif
}

static nu_u256 to_u256(nu_calc_t src) {
#if NU_HAS_NATIVE_U256
    if (src.type == NU_PREC_64) return (nu_u256)src.data.v64;
    if (src.type == NU_PREC_128) {
        #if NU_HAS_NATIVE_U128
            return (nu_u256)src.data.v128;
        #else
            nu_u128 v = src.data.v128;
            return (nu_u256)v.low | ((nu_u256)v.high << 64);
        #endif
    }
    return src.data.v256;
#else
    return to_soft256(src);
#endif
}

nu_calc_t nu_calc_from_u64(uint64_t val) {
    nu_calc_t res;
    res.type = NU_PREC_64;
    res.data.v64 = val;
    return res;
}

nu_calc_t nu_calc_add(nu_calc_t a, nu_calc_t b) {
    nu_calc_t res;
    nu_prec_type_t cur = (a.type > b.type) ? a.type : b.type;

    if (cur == NU_PREC_64) {
        uint64_t va = a.data.v64, vb = b.data.v64;
        uint64_t sum = va + vb;
        if (sum >= va) {
            res.type = NU_PREC_64;
            res.data.v64 = sum;
            return res;
        }
        cur = NU_PREC_128;
    }

    if (cur == NU_PREC_128) {
        nu_u128 va = to_u128(a), vb = to_u128(b);
        #if NU_HAS_NATIVE_U128
            nu_u128 sum = va + vb;
            if (sum >= va) {
                res.type = NU_PREC_128;
                res.data.v128 = sum;
                return res;
            }
        #else
            nu_u128 sum = soft_add128(va, vb);
            int carry = (sum.high < va.high) || (sum.high == va.high && sum.low < va.low);
            if (!carry) {
                res.type = NU_PREC_128;
                res.data.v128 = sum;
                return res;
            }
        #endif
        cur = NU_PREC_256;
    }

    res.type = NU_PREC_256;
    nu_u256 va = to_u256(a), vb = to_u256(b);
    #if NU_HAS_NATIVE_U256
        res.data.v256 = va + vb;
    #else
        res.data.v256 = soft_add256(va, vb);
    #endif
    return res;
}

nu_calc_t nu_calc_sub(nu_calc_t a, nu_calc_t b) {
    nu_calc_t res;
    nu_prec_type_t cur = (a.type > b.type) ? a.type : b.type;

    if (cur == NU_PREC_64) {
        uint64_t va = a.data.v64, vb = b.data.v64;
        if (va >= vb) {
            res.type = NU_PREC_64;
            res.data.v64 = va - vb;
            return res;
        }
        cur = NU_PREC_128;
    }

    if (cur == NU_PREC_128) {
        nu_u128 va = to_u128(a), vb = to_u128(b);
        #if NU_HAS_NATIVE_U128
            if (va >= vb) {
                res.type = NU_PREC_128;
                res.data.v128 = va - vb;
                return res;
            }
        #else
            if (soft_cmp128(va, vb) >= 0) {
                res.type = NU_PREC_128;
                res.data.v128 = soft_sub128(va, vb);
                return res;
            }
        #endif
        cur = NU_PREC_256;
    }

    res.type = NU_PREC_256;
    nu_u256 va = to_u256(a), vb = to_u256(b);
    #if NU_HAS_NATIVE_U256
        res.data.v256 = va - vb;
    #else
        res.data.v256 = soft_sub256(va, vb);
    #endif
    return res;
}

nu_calc_t nu_calc_mul(nu_calc_t a, nu_calc_t b) {
    nu_calc_t res;
    nu_prec_type_t cur = (a.type > b.type) ? a.type : b.type;

    if (cur == NU_PREC_64) {
        uint64_t va = a.data.v64, vb = b.data.v64;
        #if NU_HAS_NATIVE_U128
            nu_u128 p = (nu_u128)va * (nu_u128)vb;
            if ((p >> 64) == 0) {
                res.type = NU_PREC_64;
                res.data.v64 = (uint64_t)p;
                return res;
            }
            res.type = NU_PREC_128;
            res.data.v128 = p;
            return res;
        #else
            nu_u128 p = soft_mul64(va, vb);
            if (p.high == 0) {
                res.type = NU_PREC_64;
                res.data.v64 = p.low;
                return res;
            }
            res.type = NU_PREC_128;
            res.data.v128 = p;
            return res;
        #endif
    }

    if (cur == NU_PREC_128) {
        nu_u128 va = to_u128(a), vb = to_u128(b);
        #if NU_HAS_NATIVE_U256
            nu_u256 p = (nu_u256)va * (nu_u256)vb;
            if ((p >> 128) == 0) {
                res.type = NU_PREC_128;
                #if NU_HAS_NATIVE_U128
                    res.data.v128 = (nu_u128)p;
                #else
                    res.data.v128 = (nu_u128){(uint64_t)p, (uint64_t)(p >> 64)};
                #endif
                return res;
            }
            res.type = NU_PREC_256;
            res.data.v256 = p;
            return res;
        #else
            nu_u256 va256 = to_soft256(a), vb256 = to_soft256(b);
            nu_u256 prod = {{0, 0, 0, 0}};
            for (int i = 0; i < 4; i++) {
                uint64_t carry = 0;
                for (int j = 0; j < 4 - i; j++) {
                    #if NU_HAS_NATIVE_U128
                        __uint128_t temp = (__uint128_t)va256.limbs[i] * vb256.limbs[j] + prod.limbs[i+j] + carry;
                        prod.limbs[i+j] = (uint64_t)temp;
                        carry = (uint64_t)(temp >> 64);
                    #else
                        nu_u128 m = soft_mul64(va256.limbs[i], vb256.limbs[j]);
                        nu_u128 s = soft_add128(m, (nu_u128){prod.limbs[i+j], 0});
                        s = soft_add128(s, (nu_u128){carry, 0});
                        prod.limbs[i+j] = s.low;
                        carry = s.high;
                    #endif
                }
            }
            if (prod.limbs[2] == 0 && prod.limbs[3] == 0) {
                res.type = NU_PREC_128;
                #if NU_HAS_NATIVE_U128
                    res.data.v128 = ((nu_u128)prod.limbs[1] << 64) | prod.limbs[0];
                #else
                    res.data.v128 = (nu_u128){prod.limbs[0], prod.limbs[1]};
                #endif
                return res;
            }
            res.type = NU_PREC_256;
            res.data.v256 = prod;
            return res;
        #endif
    }

    res.type = NU_PREC_256;
    nu_u256 va = to_u256(a), vb = to_u256(b);
    #if NU_HAS_NATIVE_U256
        res.data.v256 = va * vb;
    #else
        nu_u256 prod = {{0, 0, 0, 0}};
        for (int i = 0; i < 4; i++) {
            uint64_t carry = 0;
            for (int j = 0; j < 4 - i; j++) {
                #if NU_HAS_NATIVE_U128
                    __uint128_t temp = (__uint128_t)va.limbs[i] * vb.limbs[j] + prod.limbs[i+j] + carry;
                    prod.limbs[i+j] = (uint64_t)temp;
                    carry = (uint64_t)(temp >> 64);
                #else
                    nu_u128 m = soft_mul64(va.limbs[i], vb.limbs[j]);
                    nu_u128 s = soft_add128(m, (nu_u128){prod.limbs[i+j], 0});
                    s = soft_add128(s, (nu_u128){carry, 0});
                    prod.limbs[i+j] = s.low;
                    carry = s.high;
                #endif
            }
        }
        res.data.v256 = prod;
    #endif
    return res;
}

nu_calc_t nu_calc_div(nu_calc_t a, nu_calc_t b) {
    nu_calc_t res;
    nu_prec_type_t cur = (a.type > b.type) ? a.type : b.type;
    res.type = cur;

    if (cur == NU_PREC_64) {
        if (b.data.v64 == 0) { res.data.v64 = 0; return res; }
        res.data.v64 = a.data.v64 / b.data.v64;
    }
    else if (cur == NU_PREC_128) {
        nu_u128 va = to_u128(a), vb = to_u128(b);
        #if NU_HAS_NATIVE_U128
            if (vb == 0) { res.data.v128 = 0; return res; }
            res.data.v128 = va / vb;
        #else
            res.data.v128 = soft_div128(va, vb);
        #endif
    }
    else {
        nu_u256 va = to_u256(a), vb = to_u256(b);
        #if NU_HAS_NATIVE_U256
            if (vb == 0) { res.data.v256 = 0; return res; }
            res.data.v256 = va / vb;
        #else
            res.data.v256 = soft_div256(va, vb);
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


#ifndef SIMD_H
#define SIMD_H

#include <stdint.h>
#include <immintrin.h>

// Check if AVX-512 is supported
static inline int check_avx512_support(void) {
    #ifdef __AVX512F__
    return 1;
    #else
    return 0;
    #endif
}

// SIMD-optimized hash comparison
static inline int compare_hash_simd(const uint8_t* hash1, const uint8_t* hash2) {
    #ifdef __AVX512F__
    __m512i v1 = _mm512_loadu_si512((__m512i*)hash1);
    __m512i v2 = _mm512_loadu_si512((__m512i*)hash2);
    __mmask64 mask = _mm512_cmpeq_epi8_mask(v1, v2);
    
    // If all bytes are equal, return 0
    if (mask == 0xFFFFFFFFFFFFFFFF) {
        return 0;
    }
    
    // Find first differing byte
    int first_diff = __builtin_ctzll(~mask);
    return hash1[first_diff] < hash2[first_diff] ? -1 : 1;
    #else
    // Fallback to scalar comparison
    for (int i = 0; i < 32; i++) {
        if (hash1[i] != hash2[i]) {
            return hash1[i] < hash2[i] ? -1 : 1;
        }
    }
    return 0;
    #endif
}

// SIMD-optimized memory set
static inline void memset_simd(uint8_t* dest, uint8_t val, size_t len) {
    #ifdef __AVX512F__
    size_t i = 0;
    __m512i v = _mm512_set1_epi8(val);
    
    // Process 64 bytes at a time
    for (; i + 64 <= len; i += 64) {
        _mm512_storeu_si512((__m512i*)(dest + i), v);
    }
    
    // Handle remaining bytes
    for (; i < len; i++) {
        dest[i] = val;
    }
    #else
    memset(dest, val, len);
    #endif
}

// SIMD-optimized memory copy
static inline void memcpy_simd(uint8_t* dest, const uint8_t* src, size_t len) {
    #ifdef __AVX512F__
    size_t i = 0;
    
    // Process 64 bytes at a time
    for (; i + 64 <= len; i += 64) {
        __m512i v = _mm512_loadu_si512((__m512i*)(src + i));
        _mm512_storeu_si512((__m512i*)(dest + i), v);
    }
    
    // Handle remaining bytes
    for (; i < len; i++) {
        dest[i] = src[i];
    }
    #else
    memcpy(dest, src, len);
    #endif
}

#endif // SIMD_H 
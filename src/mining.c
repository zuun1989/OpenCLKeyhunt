#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <secp256k1.h>
#include "../include/miner.h"
#include "../include/simd.h"

static secp256k1_context* ctx = NULL;
static KeypairPool* g_keypair_pool = NULL;

// 比较两个哈希值，返回 true 如果 hash1 小于 hash2
static bool is_hash_better(const uint8_t* hash1, const uint8_t* hash2) {
    return compare_hash_simd(hash1, hash2) < 0;
}

void init_mining() {
    // 初始化 OpenSSL 的随机数生成器
    RAND_seed(&ctx, sizeof(ctx)); // 使用 secp256k1 上下文作为种子
    RAND_seed(&time, sizeof(time_t)); // 使用当前时间作为种子
    
    // 创建 secp256k1 上下文
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if (!ctx) {
        printf("Failed to create secp256k1 context\n");
        exit(1);
    }
    
    // Check and report AVX-512 support
    if (check_avx512_support()) {
        printf("AVX-512 support detected and enabled\n");
    } else {
        printf("AVX-512 not supported, using scalar operations\n");
    }
    
    // Create keypair pool (1GB worth of keypairs)
    // Each keypair is 97 bytes (65 for public key + 32 for private key)
    // 1GB = 1 * 1024 * 1024 * 1024 bytes
    // Number of keypairs = 1GB / 97 bytes
    size_t keypair_size = sizeof(Keypair); // 97 bytes
    size_t num_keypairs = (1ULL * 1024 * 1024 * 1024) / keypair_size;
    
    printf("Creating keypair pool with capacity for %zu keypairs (%.2f GB)\n", 
           num_keypairs, (double)num_keypairs * keypair_size / (1024 * 1024 * 1024));
    
    g_keypair_pool = create_keypair_pool(num_keypairs);
    if (!g_keypair_pool) {
        printf("Failed to create keypair pool\n");
        exit(1);
    }
    
    // Pre-generate keypairs
    pregenerate_keypairs(g_keypair_pool, num_keypairs);
}

void cleanup_mining() {
    if (ctx) {
        secp256k1_context_destroy(ctx);
        ctx = NULL;
    }
    
    if (g_keypair_pool) {
        free_keypair_pool(g_keypair_pool);
        g_keypair_pool = NULL;
    }
}

static void sha256_hash(const uint8_t* input, size_t len, uint8_t* output) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        printf("Failed to create EVP context\n");
        exit(1);
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        printf("Failed to initialize SHA256\n");
        EVP_MD_CTX_free(ctx);
        exit(1);
    }

    if (EVP_DigestUpdate(ctx, input, len) != 1) {
        printf("Failed to update SHA256\n");
        EVP_MD_CTX_free(ctx);
        exit(1);
    }

    unsigned int digest_len;
    if (EVP_DigestFinal_ex(ctx, output, &digest_len) != 1) {
        printf("Failed to finalize SHA256\n");
        EVP_MD_CTX_free(ctx);
        exit(1);
    }

    EVP_MD_CTX_free(ctx);
}

bool mine_block(const MinerConfig* config, Job* job, Solution* solution) {
    (void)config; // Unused parameter
    uint8_t hash[32];
    char combined[1024];
    bool found_solution = false;
    
    // Get a keypair from the pool
    Keypair* keypair = get_next_keypair(g_keypair_pool);
    if (!keypair) {
        printf("Failed to get keypair from pool\n");
        return false;
    }
    
    // Convert public key to hex string
    char public_key_hex[131];  // 65 bytes * 2 chars per byte + null terminator
    for (int i = 0; i < 65; i++) {
        sprintf(public_key_hex + i*2, "%02x", keypair->public_key[i]);
    }
    public_key_hex[130] = '\0';
    
    // Combine public key hex and seed
    snprintf(combined, sizeof(combined), "%s%s", public_key_hex, job->seed);
    
    // Calculate hash
    sha256_hash((uint8_t*)combined, strlen(combined), hash);
    
    // 更新最佳哈希值（如果当前哈希值更好）
    pthread_mutex_lock(&g_hash_mutex);
    if (is_hash_better(hash, g_best_hash)) {
        memcpy_simd(g_best_hash, hash, 32);
    }
    pthread_mutex_unlock(&g_hash_mutex);
    
    // Check if hash meets difficulty using SIMD comparison
    bool meets_difficulty = true;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != job->diff[i]) {
            // 当前字节不相等时判断难度
            if (hash[i] > job->diff[i]) {
                meets_difficulty = false;
            }
            break;
        }
    }
    
    if (meets_difficulty) {
        memcpy_simd(solution->public_key, keypair->public_key, 65);
        memcpy_simd(solution->private_key, keypair->private_key, 32);
        
        // Convert hash to hex string
        char hash_hex[65];
        for (int i = 0; i < 32; i++) {
            sprintf(hash_hex + i*2, "%02x", hash[i]);
        }
        solution->hash = strdup(hash_hex);
        solution->reward = job->reward;
        found_solution = true;
    }
    
    return found_solution;
}

void print_hash_rate(uint64_t hash_count) {
    const char* unit;
    double rate;
    
    if (hash_count >= 1000000000000ULL) {
        rate = hash_count / 1000000000000.0;
        unit = "TH/s";
    } else if (hash_count >= 1000000000ULL) {
        rate = hash_count / 1000000000.0;
        unit = "GH/s";
    } else if (hash_count >= 1000000ULL) {
        rate = hash_count / 1000000.0;
        unit = "MH/s";
    } else if (hash_count >= 1000ULL) {
        rate = hash_count / 1000.0;
        unit = "KH/s";
    } else {
        rate = hash_count;
        unit = "H/s";
    }
    
    printf("\r[INFO] %.2f %s", rate, unit);
    fflush(stdout);
}

void save_reward(const MinerConfig* config, const Solution* solution, uint64_t coin_id) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/%lu.coin", config->rewards_dir, coin_id);
    
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Failed to save reward to %s\n", filename);
        return;
    }
    
    // Save private key
    for (int i = 0; i < 32; i++) {
        fprintf(fp, "%02x", solution->private_key[i]);
    }
    fprintf(fp, "\n");
    
    fclose(fp);
    
    // Execute on_mined command if specified
    if (strlen(config->on_mined) > 0) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), config->on_mined, coin_id);
        int result = system(cmd);
        if (result != 0) {
            printf("Warning: Command execution failed with code %d\n", result);
        }
    }
} 

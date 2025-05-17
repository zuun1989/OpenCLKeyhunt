#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "../include/miner.h"
#include <openssl/sha.h>
#include <secp256k1.h>

struct MemoryStruct {
    char* memory;
    size_t size;
};

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        printf("Failed to allocate memory\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static char* make_request(const char* url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        printf("Failed to initialize CURL\n");
        return NULL;
    }

    struct MemoryStruct chunk = {malloc(1), 0};
    if (!chunk.memory) {
        curl_easy_cleanup(curl);
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        printf("CURL error: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        return NULL;
    }

    return chunk.memory;
}

Job* get_job(const char* server_url) {
    char url[1024];
    snprintf(url, sizeof(url), "%s/get-challenge", server_url);

    char* response = make_request(url);
    if (!response) {
        printf("Failed to get job from server: %s\n", server_url);
        return NULL;
    }

    // Parse JSON response (simplified version)
    Job* job = malloc(sizeof(Job));
    if (!job) {
        free(response);
        return NULL;
    }

    // Initialize job structure
    job->seed = NULL;

    // Extract seed
    char* seed_start = strstr(response, "\"seed\":\"");
    if (seed_start) {
        seed_start += 8;
        char* seed_end = strchr(seed_start, '"');
        if (seed_end) {
            size_t seed_len = seed_end - seed_start;
            job->seed = malloc(seed_len + 1);
            if (job->seed) {
                strncpy(job->seed, seed_start, seed_len);
                job->seed[seed_len] = '\0';
            }
        }
    }

    // Extract difficulty
    char* diff_start = strstr(response, "\"diff\":\"");
    if (diff_start) {
        diff_start += 8;
        char* diff_end = strchr(diff_start, '"');
        if (diff_end) {
            size_t diff_len = diff_end - diff_start;
            char diff_hex[65] = {0};
            strncpy(diff_hex, diff_start, diff_len < 64 ? diff_len : 64);
            // Convert hex to bytes
            for (int i = 0; i < 32; i++) {
                sscanf(diff_hex + i*2, "%02hhx", &job->diff[i]);
            }
        }
    }

    // Extract reward
    char* reward_start = strstr(response, "\"reward\":");
    if (reward_start) {
        job->reward = atof(reward_start + 9);
    }

    // Extract last_found
    char* last_found_start = strstr(response, "\"lastFound\":");
    if (last_found_start) {
        job->last_found = strtoull(last_found_start + 12, NULL, 10);
    }

    free(response);

    // Validate job
    if (!job->seed) {
        printf("Invalid job response: missing seed\n");
        free(job);
        return NULL;
    }

    return job;
}

bool submit_solution(const MinerConfig* config, const Solution* solution) {
    char url[2048];
    char public_key_hex[131];
    char private_key_hex[65];
    char signature[145]; // For DER format signature

    // Convert solution's public key to hex
    for (int i = 0; i < 65; i++) {
        sprintf(public_key_hex + i*2, "%02x", solution->public_key[i]);
    }
    public_key_hex[130] = '\0';

    // Convert solution's private key to hex
    for (int i = 0; i < 32; i++) {
        sprintf(private_key_hex + i*2, "%02x", solution->private_key[i]);
    }
    private_key_hex[64] = '\0';

    // Convert private key from hex to bytes for signing
    unsigned char privkey_bytes[32];
    for (int i = 0; i < 32; i++) {
        sscanf(private_key_hex + i*2, "%02hhx", &privkey_bytes[i]);
    }
    
    // 1. Hash the hex-encoded public key with SHA-256
    unsigned char hash[32];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, public_key_hex, strlen(public_key_hex));
    SHA256_Final(hash, &sha256);
    
    // 2. Sign the hash with the private key using ECDSA
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_ecdsa_signature sig;
    
    if (!secp256k1_ecdsa_sign(ctx, &sig, hash, privkey_bytes, NULL, NULL)) {
        printf("%s[ERROR] Failed to sign hash%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
        secp256k1_context_destroy(ctx);
        return false;
    }
    
    // 3. Serialize the signature in DER format
    unsigned char sig_der[72];
    size_t der_len = 72;
    
    if (!secp256k1_ecdsa_signature_serialize_der(ctx, sig_der, &der_len, &sig)) {
        printf("%s[ERROR] Failed to serialize signature%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
        secp256k1_context_destroy(ctx);
        return false;
    }
    
    // Convert signature to hex
    for (int i = 0; i < der_len; i++) {
        sprintf(signature + i*2, "%02x", sig_der[i]);
    }
    signature[der_len*2] = '\0';
    
    secp256k1_context_destroy(ctx);

    if (strlen(config->pool_secret) > 0) {
        snprintf(url, sizeof(url), 
            "%s/challenge-solved?holder=%s&sign=%s&hash=%s&poolsecret=%s&key=%s",
            config->server, public_key_hex, signature, solution->hash,
            config->pool_secret, private_key_hex);
        // Print submission information
        printf("%s[INFO] Submitting solution to pool%s\n", ANSI_COLOR_BLUE, ANSI_COLOR_RESET);
    } else {
        snprintf(url, sizeof(url), 
            "%s/challenge-solved?holder=%s&sign=%s&hash=%s",
            config->server, public_key_hex, signature, solution->hash);
        printf("%s[INFO] Submitting solution%s\n", ANSI_COLOR_BLUE, ANSI_COLOR_RESET);
    }

    char* response = make_request(url);
    if (!response) {
        printf("%s[ERROR] Failed to submit solution%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
        return false;
    }

    bool success = strstr(response, "success") != NULL;
    if (!success) {
        printf("%s[ERROR] Server response: %s%s\n", ANSI_COLOR_RED, response, ANSI_COLOR_RESET);
    } else {
        printf("%s[INFO] Solution submitted successfully%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
    }
    free(response);
    return success;
} 
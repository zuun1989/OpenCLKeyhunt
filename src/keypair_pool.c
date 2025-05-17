#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <openssl/rand.h>
#include <secp256k1.h>
#include "../include/miner.h"
#include "../include/simd.h"

// Global secp256k1 context for keypair generation
static secp256k1_context* ctx = NULL;

// Thread data structure for parallel keypair generation
typedef struct {
    KeypairPool* pool;
    size_t start_index;
    size_t count;
    pthread_mutex_t* progress_mutex;
    size_t* total_generated;
    size_t total_to_generate;
} ThreadGenData;

// Create a new keypair pool with the specified capacity
KeypairPool* create_keypair_pool(size_t capacity) {
    KeypairPool* pool = (KeypairPool*)malloc(sizeof(KeypairPool));
    if (!pool) {
        printf("Failed to allocate memory for keypair pool\n");
        return NULL;
    }
    
    pool->keypairs = (Keypair*)malloc(capacity * sizeof(Keypair));
    if (!pool->keypairs) {
        printf("Failed to allocate memory for keypairs\n");
        free(pool);
        return NULL;
    }
    
    pool->size = 0;
    pool->capacity = capacity;
    pool->current_index = 0;
    pthread_mutex_init(&pool->mutex, NULL);
    
    return pool;
}

// Free a keypair pool
void free_keypair_pool(KeypairPool* pool) {
    if (pool) {
        if (pool->keypairs) {
            free(pool->keypairs);
        }
        pthread_mutex_destroy(&pool->mutex);
        free(pool);
    }
}

// Generate a single keypair
static void generate_keypair(Keypair* keypair) {
    secp256k1_pubkey pub;
    
    // Generate private key
    do {
        if (RAND_bytes(keypair->private_key, 32) != 1) {
            printf("Failed to generate random bytes\n");
            exit(1);
        }
    } while (!secp256k1_ec_seckey_verify(ctx, keypair->private_key));

    // Generate public key
    if (!secp256k1_ec_pubkey_create(ctx, &pub, keypair->private_key)) {
        printf("Failed to create public key\n");
        exit(1);
    }

    // Serialize public key
    size_t len = 65;
    if (!secp256k1_ec_pubkey_serialize(ctx, keypair->public_key, &len, &pub, SECP256K1_EC_UNCOMPRESSED)) {
        printf("Failed to serialize public key\n");
        exit(1);
    }
}

// Thread function for parallel keypair generation
static void* generate_keypairs_thread(void* arg) {
    ThreadGenData* data = (ThreadGenData*)arg;
    KeypairPool* pool = data->pool;
    size_t start_index = data->start_index;
    size_t count = data->count;
    
    // Create thread-local secp256k1 context
    secp256k1_context* thread_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if (!thread_ctx) {
        printf("Failed to create secp256k1 context for thread\n");
        return NULL;
    }
    
    // Generate keypairs in batches for better performance
    const size_t batch_size = 10000;
    size_t remaining = count;
    size_t generated = 0;
    
    while (remaining > 0) {
        size_t current_batch = (remaining < batch_size) ? remaining : batch_size;
        
        for (size_t i = 0; i < current_batch; i++) {
            // Generate keypair
            secp256k1_pubkey pub;
            
            // Generate private key
            do {
                if (RAND_bytes(pool->keypairs[start_index + generated + i].private_key, 32) != 1) {
                    printf("Failed to generate random bytes\n");
                    secp256k1_context_destroy(thread_ctx);
                    return NULL;
                }
            } while (!secp256k1_ec_seckey_verify(thread_ctx, pool->keypairs[start_index + generated + i].private_key));

            // Generate public key
            if (!secp256k1_ec_pubkey_create(thread_ctx, &pub, pool->keypairs[start_index + generated + i].private_key)) {
                printf("Failed to create public key\n");
                secp256k1_context_destroy(thread_ctx);
                return NULL;
            }

            // Serialize public key
            size_t len = 65;
            if (!secp256k1_ec_pubkey_serialize(thread_ctx, pool->keypairs[start_index + generated + i].public_key, &len, &pub, SECP256K1_EC_UNCOMPRESSED)) {
                printf("Failed to serialize public key\n");
                secp256k1_context_destroy(thread_ctx);
                return NULL;
            }
        }
        
        generated += current_batch;
        remaining -= current_batch;
        
        // Update progress
        pthread_mutex_lock(data->progress_mutex);
        *data->total_generated += current_batch;
        
        // Print progress
        if (*data->total_generated % 10000 == 0 || *data->total_generated == data->total_to_generate) {
            printf("Generated %zu/%zu keypairs (%.1f%%)\n", 
                   *data->total_generated, data->total_to_generate, 
                   (float)*data->total_generated / data->total_to_generate * 100);
            fflush(stdout);
        }
        pthread_mutex_unlock(data->progress_mutex);
    }
    
    // Clean up thread context
    secp256k1_context_destroy(thread_ctx);
    
    return NULL;
}

// Pre-generate keypairs and add them to the pool using multiple threads
void pregenerate_keypairs(KeypairPool* pool, size_t count) {
    if (!pool || !pool->keypairs) {
        return;
    }
    
    // Ensure we don't exceed capacity
    if (count > pool->capacity) {
        count = pool->capacity;
    }
    
    // Create secp256k1 context if not already created
    if (!ctx) {
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
        if (!ctx) {
            printf("Failed to create secp256k1 context\n");
            exit(1);
        }
    }
    
    printf("Pre-generating %zu keypairs using parallel threads...\n", count);
    
    // Determine number of threads to use
    int num_threads = 24;
    
    printf("Using %d threads for keypair generation\n", num_threads);
    
    // Create threads
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    ThreadGenData* thread_data = (ThreadGenData*)malloc(num_threads * sizeof(ThreadGenData));
    
    if (!threads || !thread_data) {
        printf("Failed to allocate memory for threads\n");
        free(threads);
        free(thread_data);
        return;
    }
    
    // Initialize progress tracking
    pthread_mutex_t progress_mutex = PTHREAD_MUTEX_INITIALIZER;
    size_t total_generated = 0;
    
    // Calculate keypairs per thread
    size_t keypairs_per_thread = count / num_threads;
    size_t remaining_keypairs = count % num_threads;
    
    // Create and start threads
    size_t start_index = 0;
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].pool = pool;
        thread_data[i].start_index = start_index;
        thread_data[i].count = keypairs_per_thread + (i < remaining_keypairs ? 1 : 0);
        thread_data[i].progress_mutex = &progress_mutex;
        thread_data[i].total_generated = &total_generated;
        thread_data[i].total_to_generate = count;
        
        if (pthread_create(&threads[i], NULL, generate_keypairs_thread, &thread_data[i]) != 0) {
            printf("Failed to create thread %d\n", i);
            // Clean up and exit
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            pthread_mutex_destroy(&progress_mutex);
            free(threads);
            free(thread_data);
            return;
        }
        
        start_index += thread_data[i].count;
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Clean up
    pthread_mutex_destroy(&progress_mutex);
    free(threads);
    free(thread_data);
    
    pool->size = count;
    printf("Keypair generation complete. Pool size: %zu\n", pool->size);
}

// Get the next keypair from the pool
Keypair* get_next_keypair(KeypairPool* pool) {
    if (!pool || !pool->keypairs || pool->size == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    // Get the current keypair
    Keypair* keypair = &pool->keypairs[pool->current_index];
    
    // Update the index for the next call
    pool->current_index = (pool->current_index + 1) % pool->size;
    
    pthread_mutex_unlock(&pool->mutex);
    
    return keypair;
} 
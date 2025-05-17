#ifndef MINER_H
#define MINER_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <pthread.h>

// Color definitions
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Global variables for thread safety
extern uint8_t* g_best_hash;
extern pthread_mutex_t g_hash_mutex;

// Keypair structure
typedef struct {
    uint8_t public_key[65];  // Uncompressed public key
    uint8_t private_key[32]; // Private key
} Keypair;

// Keypair pool structure
typedef struct {
    Keypair* keypairs;
    size_t size;
    size_t capacity;
    size_t current_index;
    pthread_mutex_t mutex;
} KeypairPool;

// Configuration structure
typedef struct {
    char* server;
    char* rewards_dir;
    char* on_mined;
    int thread_count;
    int job_interval;
    int report_interval;
    struct {
        char* report_server;
        char* report_user;
    } reporting;
    char* pool_secret;
} MinerConfig;

// Job structure
typedef struct {
    char* seed;
    uint8_t diff[32];  // 256-bit difficulty
    double reward;
    uint64_t last_found;
} Job;

// Solution structure
typedef struct {
    uint8_t public_key[65];  // Uncompressed public key
    uint8_t private_key[32]; // Private key
    char* hash;
    double reward;
} Solution;

// Function declarations
MinerConfig* load_config(const char* config_file);
void free_config(MinerConfig* config);
Job* get_job(const char* server_url);
bool submit_solution(const MinerConfig* config, const Solution* solution);
bool mine_block(const MinerConfig* config, Job* job, Solution* solution);
void print_hash_rate(uint64_t hash_count);
void save_reward(const MinerConfig* config, const Solution* solution, uint64_t coin_id);
bool report_status(const MinerConfig* config, uint64_t hash_count, double total_mined, const uint8_t* best_hash);

// Mining context management
void init_mining(void);
void cleanup_mining(void);

// Keypair pool management
KeypairPool* create_keypair_pool(size_t capacity);
void free_keypair_pool(KeypairPool* pool);
void pregenerate_keypairs(KeypairPool* pool, size_t count);
Keypair* get_next_keypair(KeypairPool* pool);

#endif // MINER_H 
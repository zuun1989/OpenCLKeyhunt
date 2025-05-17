#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <curl/curl.h>
#include "../include/miner.h"

// Global variables
uint8_t* g_best_hash = NULL;
pthread_mutex_t g_hash_mutex = PTHREAD_MUTEX_INITIALIZER;

// Color definitions
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define MAX_THREADS 384

typedef struct {
    const MinerConfig* config;
    Job* job;
    uint64_t* hash_count;
    double* total_mined;
    pthread_mutex_t* job_mutex;
} ThreadData;

static void* mining_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    Solution solution = {0};
    uint64_t local_hash_count = 0;
    
    while (1) {
        pthread_mutex_lock(data->job_mutex);
        if (!data->job->seed || strcmp(data->job->seed, "wait") == 0) {
            pthread_mutex_unlock(data->job_mutex);
            usleep(100000);  // Sleep 100ms
            continue;
        }
        Job current_job = *data->job;
        pthread_mutex_unlock(data->job_mutex);
        
        if (mine_block(data->config, &current_job, &solution)) {
            printf("\n\n%s[INFO] Found %.2f CLCs!%s\n", ANSI_COLOR_GREEN, solution.reward, ANSI_COLOR_RESET);
            printf("%s[INFO] Hash: %s%s\n", ANSI_COLOR_CYAN, solution.hash, ANSI_COLOR_RESET);
            
            if (submit_solution(data->config, &solution)) {
                printf("%s[INFO] Successfully submitted.%s\n\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
                *data->total_mined += solution.reward;
                
                // Save reward
                save_reward(data->config, &solution, time(NULL));
            }
            
            free(solution.hash);
            memset(&solution, 0, sizeof(Solution));
        }
        
        local_hash_count++;
        if (local_hash_count % 100 == 0) {
            pthread_mutex_lock(&g_hash_mutex);
            *data->hash_count += 100;
            pthread_mutex_unlock(&g_hash_mutex);
            local_hash_count = 0;
        }
    }
    
    return NULL;
}

static void* job_update_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    while (1) {
        Job* new_job = get_job(data->config->server);
        if (new_job) {
            pthread_mutex_lock(data->job_mutex);
            
            
            // 检查job是否变化
            if (!data->job->seed || !new_job->seed || strcmp(data->job->seed, new_job->seed) != 0) {
                
                printf("\n\n%s[INFO] New job%s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);
                printf("%s[INFO] seed: %s%s\n", ANSI_COLOR_CYAN, new_job->seed, ANSI_COLOR_RESET);
                printf("%s[INFO] diff: ", ANSI_COLOR_CYAN);
                for(int i = 0; i < 32; i++) {
                    printf("%02x", new_job->diff[i]);
                }
                printf("%s\n", ANSI_COLOR_RESET);
                printf("%s[INFO] reward: %.2f%s\n", ANSI_COLOR_GREEN, new_job->reward, ANSI_COLOR_RESET);
                
                time_t now = time(NULL);
                time_t last_found = new_job->last_found / 1000;
                printf("%s[INFO] Last mined %lds ago%s\n\n", ANSI_COLOR_BLUE, now - last_found, ANSI_COLOR_RESET);
                
                // Free old job seed
                if (data->job->seed) {
                    free(data->job->seed);
                }
                
                // Copy new job
                *data->job = *new_job;
                
                // Prevent double free
                new_job->seed = NULL;
                
                // 重置最佳哈希为全F
                pthread_mutex_lock(&g_hash_mutex);
                memset(g_best_hash, 0xFF, 32);
                pthread_mutex_unlock(&g_hash_mutex);
            }
            
            pthread_mutex_unlock(data->job_mutex);
            
            // Clean up new job
            if (new_job->seed) {
                free(new_job->seed);
            }
            free(new_job);
        }
        
        // 打印当前时间和等待间隔
        time_t now = time(NULL);
        struct tm *timeinfo = localtime(&now);
        printf("\n%s[INFO] Current time: %02d:%02d:%02d, waiting for %d seconds...%s\n",
            ANSI_COLOR_YELLOW,
            timeinfo->tm_hour,
            timeinfo->tm_min, 
            timeinfo->tm_sec,
            data->config->job_interval,
            ANSI_COLOR_RESET);
        sleep(data->config->job_interval);
    }
    
    return NULL;
}

static void* hash_rate_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    while (1) {
        sleep(3);
        pthread_mutex_lock(&g_hash_mutex);
        print_hash_rate(*data->hash_count);
        *data->hash_count = 0;
        pthread_mutex_unlock(&g_hash_mutex);
    }
    
    return NULL;
}

static void* best_hash_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    while (1) {
        sleep(10); // 每10秒打印一次最佳哈希值
        
        pthread_mutex_lock(data->job_mutex);
        double total_mined = *data->total_mined;
        pthread_mutex_unlock(data->job_mutex);
        
        // 打印最佳哈希值
        // printf("\n%s[INFO] Best hash: ", ANSI_COLOR_MAGENTA);
        // for(int i = 0; i < 32; i++) {
        //     printf("%02x", g_best_hash[i]);
        // }
        // printf("%s\n", ANSI_COLOR_RESET);
        
        printf("%s[INFO] Total mined: %.2f CLCs%s\n", ANSI_COLOR_MAGENTA, total_mined, ANSI_COLOR_RESET);
           
    }
    
    return NULL;
}

static void* report_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    while (1) {
        sleep(data->config->report_interval);
        
        pthread_mutex_lock(&g_hash_mutex);
        uint64_t hash_count = *data->hash_count;
        *data->hash_count = 0;
        pthread_mutex_unlock(&g_hash_mutex);
        
        pthread_mutex_lock(data->job_mutex);
        double total_mined = *data->total_mined;
        pthread_mutex_unlock(data->job_mutex);
        
        // 报告状态
        if (strlen(data->config->reporting.report_server) > 0) {
            if (!report_status(data->config, hash_count, total_mined, g_best_hash)) {
                printf("%s[ERROR] Failed to report status%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
            } else {
                printf("%s[INFO] Status reported successfully%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
            }
        }
    }
    
    return NULL;
}

int main() {
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_ALL);
    
    // Initialize mining context
    init_mining();
    
    // Load configuration
    MinerConfig* config = load_config("clcminer.toml");
    if (!config) {
        printf("Failed to load configuration\n");
        return 1;
    }
    
    // Create rewards directory if it doesn't exist
    if (access(config->rewards_dir, F_OK) != 0) {
        #ifdef _WIN32
        mkdir(config->rewards_dir);
        #else
        mkdir(config->rewards_dir, 0755);
        #endif
    }
    
    // 打印报告服务器信息
    if (strlen(config->reporting.report_server) > 0) {
        printf("%s[INFO] Going to report to: %s/report%s\n", 
            ANSI_COLOR_BLUE, config->reporting.report_server, ANSI_COLOR_RESET);
    }
    
    // Initialize thread data
    ThreadData thread_data = {
        .config = config,
        .job = malloc(sizeof(Job)),
        .hash_count = malloc(sizeof(uint64_t)),
        .total_mined = malloc(sizeof(double)),
        .job_mutex = malloc(sizeof(pthread_mutex_t))
    };
    
    if (!thread_data.job || !thread_data.hash_count || !thread_data.total_mined || !thread_data.job_mutex) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    // Initialize best hash
    g_best_hash = malloc(32);
    if (!g_best_hash) {
        printf("Failed to allocate memory for best hash\n");
        return 1;
    }
    memset(g_best_hash, 0xFF, 32); // Initialize with maximum value
    
    // Initialize job
    thread_data.job->seed = strdup("wait");
    memset(thread_data.job->diff, 0, 32);
    thread_data.job->reward = 0;
    thread_data.job->last_found = 0;
    
    // Initialize counters
    *thread_data.hash_count = 0;
    *thread_data.total_mined = 0;
    
    // Initialize mutexes
    pthread_mutex_init(thread_data.job_mutex, NULL);
    
    // Determine number of threads
    int thread_count = config->thread_count;
    if (thread_count < 0) {
        thread_count = sysconf(_SC_NPROCESSORS_ONLN);
    }
    if (thread_count > MAX_THREADS) {
        thread_count = MAX_THREADS;
    }
    printf("%s[INFO] Using %d threads%s\n", ANSI_COLOR_BLUE, thread_count, ANSI_COLOR_RESET);
    
    // Create threads
    pthread_t threads[MAX_THREADS + 4];  // +4 for job update, hash rate, best hash, and report threads
    
    // Create mining threads
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&threads[i], NULL, mining_thread, &thread_data) != 0) {
            printf("Failed to create mining thread %d\n", i);
            return 1;
        }
    }
    
    // Create job update thread
    if (pthread_create(&threads[thread_count], NULL, job_update_thread, &thread_data) != 0) {
        printf("Failed to create job update thread\n");
        return 1;
    }
    
    // Create hash rate thread
    if (pthread_create(&threads[thread_count + 1], NULL, hash_rate_thread, &thread_data) != 0) {
        printf("Failed to create hash rate thread\n");
        return 1;
    }
    
    // Create best hash thread
    if (pthread_create(&threads[thread_count + 2], NULL, best_hash_thread, &thread_data) != 0) {
        printf("Failed to create best hash thread\n");
        return 1;
    }
    
    // Create report thread
    if (pthread_create(&threads[thread_count + 3], NULL, report_thread, &thread_data) != 0) {
        printf("Failed to create report thread\n");
        return 1;
    }
    
    // Wait for threads
    for (int i = 0; i < thread_count + 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Cleanup
    pthread_mutex_destroy(thread_data.job_mutex);
    pthread_mutex_destroy(&g_hash_mutex);
    if (thread_data.job->seed) {
        free(thread_data.job->seed);
    }
    free(thread_data.job);
    free(thread_data.hash_count);
    free(thread_data.total_mined);
    free(thread_data.job_mutex);
    free(g_best_hash);
    free_config(config);
    cleanup_mining();
    
    // Cleanup CURL
    curl_global_cleanup();
    
    return 0;
} 
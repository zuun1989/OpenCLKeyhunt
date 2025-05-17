#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/miner.h"

#define MAX_LINE_LENGTH 1024
#define MAX_CONFIG_SIZE 4096

static char* trim(char* str) {
    char* end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static char* get_value(const char* line) {
    const char* equal = strchr(line, '=');
    if (!equal) return NULL;
    
    // 获取等号后的值并去除前后空格
    char* value = trim((char*)(equal + 1));
    
    // 检查是否有引号包裹
    size_t len = strlen(value);
    if (len >= 2 && value[0] == '"' && value[len-1] == '"') {
        // 去除首尾的引号
        value[len-1] = '\0';  // 移除结尾的引号
        return value + 1;     // 跳过开头的引号
    }
    
    return value;
}

MinerConfig* load_config(const char* config_file) {
    FILE* fp = fopen(config_file, "r");
    if (!fp) {
        printf("[WARN] Config file not found, using default values...\n");
        MinerConfig* config = malloc(sizeof(MinerConfig));
        if (!config) return NULL;

        config->server = strdup("https://clc.ix.tc");
        config->rewards_dir = strdup("./rewards");
        config->thread_count = -1;
        config->job_interval = 1;
        config->report_interval = 10;
        config->on_mined = strdup("");
        config->reporting.report_server = strdup("");
        config->reporting.report_user = strdup("");
        config->pool_secret = strdup("");
        
        return config;
    }

    MinerConfig* config = malloc(sizeof(MinerConfig));
    if (!config) {
        fclose(fp);
        return NULL;
    }

    // Initialize with default values
    config->server = strdup("https://clc.ix.tc");
    config->rewards_dir = strdup("./rewards");
    config->thread_count = -1;
    config->job_interval = 1;
    config->report_interval = 10;
    config->on_mined = strdup("");
    config->reporting.report_server = strdup("");
    config->reporting.report_user = strdup("");
    config->pool_secret = strdup("");

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), fp)) {
        char* trimmed = trim(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') continue;

        // 这里的比较逻辑保持不变，因为我们在 get_value 中处理了引号
        if (strncmp(trimmed, "server =", 8) == 0) {
            free(config->server);
            config->server = strdup(get_value(trimmed));
        }
        else if (strncmp(trimmed, "rewards_dir =", 13) == 0) {
            free(config->rewards_dir);
            config->rewards_dir = strdup(get_value(trimmed));
        }
        else if (strncmp(trimmed, "thread =", 8) == 0) {
            config->thread_count = atoi(get_value(trimmed));
        }
        else if (strncmp(trimmed, "job_interval =", 14) == 0) {
            config->job_interval = atoi(get_value(trimmed));
        }
        else if (strncmp(trimmed, "report_interval =", 16) == 0) {
            config->report_interval = atoi(get_value(trimmed));
        }
        else if (strncmp(trimmed, "on_mined =", 10) == 0) {
            free(config->on_mined);
            config->on_mined = strdup(get_value(trimmed));
        }
        else if (strncmp(trimmed, "report_server =", 14) == 0) {
            free(config->reporting.report_server);
            config->reporting.report_server = strdup(get_value(trimmed));
        }
        else if (strncmp(trimmed, "report_user =", 12) == 0) {
            free(config->reporting.report_user);
            config->reporting.report_user = strdup(get_value(trimmed));
        }
        else if (strncmp(trimmed, "pool_secret =", 13) == 0) {
            free(config->pool_secret);
            config->pool_secret = strdup(get_value(trimmed));
        }
    }

    // 打印所有配置项
    printf("当前配置信息：\n");
    printf("server = %s\n", config->server);
    printf("rewards_dir = %s\n", config->rewards_dir);
    printf("thread = %d\n", config->thread_count);
    printf("job_interval = %d\n", config->job_interval);
    printf("report_interval = %d\n", config->report_interval);
    printf("on_mined = %s\n", config->on_mined);
    printf("report_server = %s\n", config->reporting.report_server);
    printf("report_user = %s\n", config->reporting.report_user);
    printf("pool_secret = %s\n", config->pool_secret);


    fclose(fp);
    return config;
}

void free_config(MinerConfig* config) {
    if (!config) return;
    
    free(config->server);
    free(config->rewards_dir);
    free(config->on_mined);
    free(config->reporting.report_server);
    free(config->reporting.report_user);
    free(config->pool_secret);
    free(config);
}
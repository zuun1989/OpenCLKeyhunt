#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "../include/miner.h"

// 报告函数，向服务器报告挖矿状态
bool report_status(const MinerConfig* config, uint64_t hash_count, double total_mined, const uint8_t* best_hash) {
    // 如果报告服务器为空，则不报告
    if (!config->reporting.report_server || strlen(config->reporting.report_server) == 0) {
        return true;
    }
    
    // 如果报告用户为空，则不报告
    if (!config->reporting.report_user || strlen(config->reporting.report_user) == 0) {
        return true;
    }
    
    // 将哈希率转换为每秒
    double hash_rate = (double)hash_count / 3.0; // 假设每3秒报告一次
    
    // 将最佳哈希转换为十六进制字符串
    char best_hash_hex[65] = {0};
    for (int i = 0; i < 32; i++) {
        sprintf(best_hash_hex + i*2, "%02x", best_hash[i]);
    }
    
    // 构建URL
    char url[2048];
    snprintf(url, sizeof(url), 
        "%s/report?user=%s&speed=%.2f&best=%s&mined=%.2f",
        config->reporting.report_server,
        config->reporting.report_user,
        hash_rate,
        best_hash_hex,
        total_mined);
    
    // 发送请求
    CURL* curl = curl_easy_init();
    if (!curl) {
        printf("%s[ERROR] Failed to initialize CURL for reporting%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
        return false;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        printf("%s[ERROR] Failed to report status: %s%s\n", 
            ANSI_COLOR_RED, curl_easy_strerror(res), ANSI_COLOR_RESET);
        return false;
    }
    
    return true;
} 
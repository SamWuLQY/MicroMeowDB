#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <io.h>
#include <sys/stat.h>

// 日志级别字符串映射
static const char *log_level_strings[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

// 默认日志配置
static const log_config default_log_config = {
    .log_path = "micromeowdb.log",
    .min_level = LOG_LEVEL_INFO,
    .target = LOG_TARGET_BOTH,
    .log_rotation = true,
    .max_log_size = 104857600, // 100MB
    .max_log_files = 10
};

// 获取当前时间字符串
static void get_time_string(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// 打开日志文件
static FILE *open_log_file(const char *path, bool *exists) {
    struct stat file_stat;
    *exists = (stat(path, &file_stat) == 0);
    
    FILE *fp = fopen(path, "a");
    if (!fp) {
        return NULL;
    }
    
    return fp;
}

// 执行日志轮转
static bool rotate_log_file(logging_system *logging) {
    if (!logging || !logging->log_path) {
        return false;
    }
    
    // 关闭当前日志文件
    if (logging->log_file) {
        fclose(logging->log_file);
        logging->log_file = NULL;
    }
    
    // 构建新的日志文件名
    char new_log_path[1024];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    snprintf(new_log_path, sizeof(new_log_path), "%s.%d%02d%02d_%02d%02d%02d.%u",
             logging->log_path, tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, logging->log_file_count);
    
    // 重命名当前日志文件
    if (rename(logging->log_path, new_log_path) != 0) {
        // 如果重命名失败，尝试直接打开新文件
        logging->log_file = fopen(logging->log_path, "w");
        if (!logging->log_file) {
            return false;
        }
    } else {
        // 打开新的日志文件
        logging->log_file = fopen(logging->log_path, "w");
        if (!logging->log_file) {
            return false;
        }
    }
    
    logging->log_file_count++;
    logging->current_log_size = 0;
    
    // 记录日志轮转信息
    char time_str[32];
    get_time_string(time_str, sizeof(time_str));
    fprintf(logging->log_file, "[%s] [INFO] Log rotation performed. New log file created.\n", time_str);
    fflush(logging->log_file);
    
    return true;
}

// 检查并执行日志轮转
bool logging_check_rotation(logging_system *logging) {
    if (!logging || !logging->log_rotation || !logging->log_file) {
        return false;
    }
    
    if (logging->current_log_size >= logging->max_log_size) {
        return rotate_log_file(logging);
    }
    
    return false;
}

// 记录日志
static void logging_log(logging_system *logging, log_level level, const char *format, va_list args) {
    if (!logging || !logging->initialized) {
        return;
    }
    
    // 检查日志级别
    if (level < logging->min_level) {
        return;
    }
    
    // 检查日志轮转
    logging_check_rotation(logging);
    
    // 获取时间字符串
    char time_str[32];
    get_time_string(time_str, sizeof(time_str));
    
    // 构建日志消息
    char message[4096];
    vsnprintf(message, sizeof(message), format, args);
    
    // 构建完整日志行
    char log_line[4096 + 64];
    snprintf(log_line, sizeof(log_line), "[%s] [%s] %s\n", time_str, log_level_strings[level], message);
    
    // 计算日志大小
    size_t log_line_size = strlen(log_line);
    logging->current_log_size += log_line_size;
    
    // 输出到文件
    if (logging->log_file && (logging->target == LOG_TARGET_FILE || logging->target == LOG_TARGET_BOTH)) {
        fwrite(log_line, 1, log_line_size, logging->log_file);
        fflush(logging->log_file);
    }
    
    // 输出到控制台
    if (logging->target == LOG_TARGET_CONSOLE || logging->target == LOG_TARGET_BOTH) {
        // 根据日志级别设置控制台颜色
        if (level >= LOG_LEVEL_ERROR) {
            fprintf(stderr, "%s", log_line);
        } else {
            fprintf(stdout, "%s", log_line);
        }
        fflush(stdout);
        fflush(stderr);
    }
}

// 初始化日志系统
logging_system *logging_init(const log_config *config) {
    logging_system *logging = (logging_system *)malloc(sizeof(logging_system));
    if (!logging) {
        return NULL;
    }
    
    // 使用默认配置或用户配置
    const log_config *used_config = config ? config : &default_log_config;
    
    // 复制日志路径
    logging->log_path = used_config->log_path ? strdup(used_config->log_path) : strdup(default_log_config.log_path);
    if (!logging->log_path) {
        free(logging);
        return NULL;
    }
    
    // 设置配置
    logging->min_level = used_config->min_level;
    logging->target = used_config->target;
    logging->log_rotation = used_config->log_rotation;
    logging->max_log_size = used_config->max_log_size;
    logging->log_file_count = 0;
    logging->current_log_size = 0;
    
    // 打开日志文件
    bool exists;
    logging->log_file = open_log_file(logging->log_path, &exists);
    if (!logging->log_file) {
        free(logging->log_path);
        free(logging);
        return NULL;
    }
    
    // 如果文件已存在，获取其大小
    if (exists) {
        fseek(logging->log_file, 0, SEEK_END);
        logging->current_log_size = ftell(logging->log_file);
        fseek(logging->log_file, 0, SEEK_END);
    }
    
    logging->initialized = true;
    
    // 记录初始化信息
    char time_str[32];
    get_time_string(time_str, sizeof(time_str));
    fprintf(logging->log_file, "[%s] [INFO] Logging system initialized. Log level: %s\n", 
            time_str, log_level_strings[logging->min_level]);
    fflush(logging->log_file);
    
    return logging;
}

// 销毁日志系统
void logging_destroy(logging_system *logging) {
    if (!logging) {
        return;
    }
    
    if (logging->log_file) {
        char time_str[32];
        get_time_string(time_str, sizeof(time_str));
        fprintf(logging->log_file, "[%s] [INFO] Logging system shutdown.\n", time_str);
        fflush(logging->log_file);
        fclose(logging->log_file);
    }
    
    if (logging->log_path) {
        free(logging->log_path);
    }
    
    free(logging);
}

// 设置日志级别
void logging_set_level(logging_system *logging, log_level level) {
    if (logging && logging->initialized) {
        logging->min_level = level;
        logging_info(logging, "Log level changed to %s", log_level_strings[level]);
    }
}

// 设置日志目标
void logging_set_target(logging_system *logging, log_target target) {
    if (logging && logging->initialized) {
        logging->target = target;
        logging_info(logging, "Log target changed");
    }
}

// 记录调试日志
void logging_debug(logging_system *logging, const char *format, ...) {
    if (!logging || !logging->initialized) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    logging_log(logging, LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

// 记录信息日志
void logging_info(logging_system *logging, const char *format, ...) {
    if (!logging || !logging->initialized) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    logging_log(logging, LOG_LEVEL_INFO, format, args);
    va_end(args);
}

// 记录警告日志
void logging_warning(logging_system *logging, const char *format, ...) {
    if (!logging || !logging->initialized) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    logging_log(logging, LOG_LEVEL_WARNING, format, args);
    va_end(args);
}

// 记录错误日志
void logging_error(logging_system *logging, const char *format, ...) {
    if (!logging || !logging->initialized) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    logging_log(logging, LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

// 记录致命日志
void logging_fatal(logging_system *logging, const char *format, ...) {
    if (!logging || !logging->initialized) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    logging_log(logging, LOG_LEVEL_FATAL, format, args);
    va_end(args);
    
    // 致命错误后刷新日志
    logging_flush(logging);
}

// 强制刷新日志
void logging_flush(logging_system *logging) {
    if (logging && logging->initialized && logging->log_file) {
        fflush(logging->log_file);
    }
}

// 获取日志级别字符串
const char *logging_level_to_string(log_level level) {
    if (level >= LOG_LEVEL_DEBUG && level <= LOG_LEVEL_FATAL) {
        return log_level_strings[level];
    }
    return "UNKNOWN";
}

// 获取当前日志文件大小
uint64_t logging_get_current_size(logging_system *logging) {
    if (!logging || !logging->initialized) {
        return 0;
    }
    return logging->current_log_size;
}

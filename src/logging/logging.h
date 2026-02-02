#ifndef LOGGING_H
#define LOGGING_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// 日志级别
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level;

// 日志输出目标
typedef enum {
    LOG_TARGET_FILE,
    LOG_TARGET_CONSOLE,
    LOG_TARGET_BOTH
} log_target;

// 日志系统结构
typedef struct {
    FILE *log_file;
    char *log_path;
    log_level min_level;
    log_target target;
    bool initialized;
    bool log_rotation;
    uint64_t max_log_size;
    uint64_t current_log_size;
    uint32_t log_file_count;
} logging_system;

// 日志配置结构
typedef struct {
    char *log_path;
    log_level min_level;
    log_target target;
    bool log_rotation;
    uint64_t max_log_size; // in bytes
    uint32_t max_log_files;
} log_config;

// 初始化日志系统
logging_system *logging_init(const log_config *config);

// 销毁日志系统
void logging_destroy(logging_system *logging);

// 设置日志级别
void logging_set_level(logging_system *logging, log_level level);

// 设置日志目标
void logging_set_target(logging_system *logging, log_target target);

// 记录调试日志
void logging_debug(logging_system *logging, const char *format, ...);

// 记录信息日志
void logging_info(logging_system *logging, const char *format, ...);

// 记录警告日志
void logging_warning(logging_system *logging, const char *format, ...);

// 记录错误日志
void logging_error(logging_system *logging, const char *format, ...);

// 记录致命日志
void logging_fatal(logging_system *logging, const char *format, ...);

// 强制刷新日志
void logging_flush(logging_system *logging);

// 检查并执行日志轮转
bool logging_check_rotation(logging_system *logging);

// 获取日志级别字符串
const char *logging_level_to_string(log_level level);

// 获取当前日志文件大小
uint64_t logging_get_current_size(logging_system *logging);

#endif // LOGGING_H

#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

// 错误描述映射
static struct {
    error_code code;
    const char *description;
} error_descriptions[] = {
    // 通用错误
    { ERROR_SUCCESS, "Operation successful" },
    { ERROR_UNKNOWN, "Unknown error" },
    { ERROR_OUT_OF_MEMORY, "Out of memory" },
    { ERROR_INVALID_PARAMETER, "Invalid parameter" },
    { ERROR_NOT_FOUND, "Resource not found" },
    { ERROR_ALREADY_EXISTS, "Resource already exists" },
    { ERROR_PERMISSION_DENIED, "Permission denied" },
    { ERROR_OPERATION_FAILED, "Operation failed" },
    { ERROR_TIMEOUT, "Operation timed out" },
    
    // 内存错误
    { ERROR_MEMORY_ALLOCATION_FAILED, "Memory allocation failed" },
    { ERROR_MEMORY_POOL_FULL, "Memory pool full" },
    { ERROR_MEMORY_CORRUPTION, "Memory corruption detected" },
    
    // 存储错误
    { ERROR_STORAGE_IO_FAILED, "Storage I/O failed" },
    { ERROR_STORAGE_DISK_FULL, "Storage disk full" },
    { ERROR_STORAGE_FILE_CORRUPT, "Storage file corrupted" },
    { ERROR_STORAGE_LOCK_FAILED, "Storage lock failed" },
    
    // 索引错误
    { ERROR_INDEX_CREATION_FAILED, "Index creation failed" },
    { ERROR_INDEX_CORRUPT, "Index corrupted" },
    { ERROR_INDEX_NOT_FOUND, "Index not found" },
    
    // 安全错误
    { ERROR_SECURITY_AUTH_FAILED, "Authentication failed" },
    { ERROR_SECURITY_INVALID_CREDENTIALS, "Invalid credentials" },
    { ERROR_SECURITY_SSL_ERROR, "SSL error" },
    { ERROR_SECURITY_PERMISSION_DENIED, "Security permission denied" },
    
    // 网络错误
    { ERROR_NETWORK_CONNECTION_FAILED, "Network connection failed" },
    { ERROR_NETWORK_SOCKET_ERROR, "Network socket error" },
    { ERROR_NETWORK_TIMEOUT, "Network timeout" },
    { ERROR_NETWORK_PACKET_CORRUPT, "Network packet corrupted" },
    
    // 配置错误
    { ERROR_CONFIG_PARSE_FAILED, "Configuration parse failed" },
    { ERROR_CONFIG_INVALID_VALUE, "Invalid configuration value" },
    { ERROR_CONFIG_MISSING_REQUIRED, "Missing required configuration" },
    
    // 事务错误
    { ERROR_TRANSACTION_BEGIN_FAILED, "Transaction begin failed" },
    { ERROR_TRANSACTION_COMMIT_FAILED, "Transaction commit failed" },
    { ERROR_TRANSACTION_ROLLBACK_FAILED, "Transaction rollback failed" },
    { ERROR_TRANSACTION_DEADLOCK, "Transaction deadlock detected" },
    { ERROR_TRANSACTION_TIMEOUT, "Transaction timeout" },
    
    // 资源错误
    { ERROR_RESOURCE_LIMIT_EXCEEDED, "Resource limit exceeded" },
    { ERROR_RESOURCE_INSUFFICIENT, "Insufficient resources" },
    { ERROR_RESOURCE_ALLOCATION_FAILED, "Resource allocation failed" },
    
    // 优化器错误
    { ERROR_OPTIMIZER_PARSE_FAILED, "Query parse failed" },
    { ERROR_OPTIMIZER_PLAN_FAILED, "Query plan creation failed" },
    { ERROR_OPTIMIZER_EXECUTE_FAILED, "Query execution failed" },
    { ERROR_OPTIMIZER_STATISTICS_FAILED, "Statistics update failed" },
    
    // 存储过程和触发器错误
    { ERROR_PROCEDURE_CREATE_FAILED, "Procedure creation failed" },
    { ERROR_PROCEDURE_EXECUTE_FAILED, "Procedure execution failed" },
    { ERROR_PROCEDURE_DROP_FAILED, "Procedure drop failed" },
    { ERROR_TRIGGER_CREATE_FAILED, "Trigger creation failed" },
    { ERROR_TRIGGER_FIRE_FAILED, "Trigger execution failed" },
    { ERROR_TRIGGER_DROP_FAILED, "Trigger drop failed" },
    
    // 复制错误
    { ERROR_REPLICATION_CONNECT_FAILED, "Replication connection failed" },
    { ERROR_REPLICATION_SYNC_FAILED, "Replication sync failed" },
    { ERROR_REPLICATION_BINLOG_FAILED, "Binlog operation failed" },
    { ERROR_REPLICATION_ROLE_CONFLICT, "Replication role conflict" },
    { ERROR_REPLICATION_SLAVE_LIMIT, "Replication slave limit exceeded" },
    
    // 客户端错误
    { ERROR_CLIENT_CONNECT_FAILED, "Client connection failed" },
    { ERROR_CLIENT_EXECUTE_FAILED, "Client execute failed" },
    { ERROR_CLIENT_PARSE_FAILED, "Client parse failed" },
    { ERROR_CLIENT_COMMAND_NOT_FOUND, "Client command not found" },
    { ERROR_CLIENT_CONNECTION_LIMIT, "Client connection limit exceeded" },
    
    // 测试错误
    { ERROR_SKIP, "Test skipped" },
    { ERROR_FAIL, "Test failed" },
    
    { 0, NULL } // 结束标记
};

// 致命错误码列表
static error_code fatal_errors[] = {
    ERROR_OUT_OF_MEMORY,
    ERROR_MEMORY_CORRUPTION,
    ERROR_STORAGE_DISK_FULL,
    ERROR_STORAGE_FILE_CORRUPT,
    ERROR_SECURITY_AUTH_FAILED,
    ERROR_TRANSACTION_DEADLOCK,
    0 // 结束标记
};

// 初始化错误处理系统
error_system *error_init(uint32_t max_errors) {
    if (max_errors == 0) {
        max_errors = 1024;
    }
    
    error_system *error_sys = (error_system *)malloc(sizeof(error_system));
    if (!error_sys) {
        return NULL;
    }
    
    error_sys->error_queue = NULL;
    error_sys->error_count = 0;
    error_sys->max_errors = max_errors;
    error_sys->initialized = true;
    
    return error_sys;
}

// 销毁错误处理系统
void error_destroy(error_system *error_sys) {
    if (!error_sys) {
        return;
    }
    
    // 清空错误队列
    error_clear_queue(error_sys);
    free(error_sys);
}

// 创建错误信息
static error_info *create_error_info(error_module module, error_level level, error_code code, const char *file, int line, const char *message) {
    error_info *info = (error_info *)malloc(sizeof(error_info));
    if (!info) {
        return NULL;
    }
    
    info->code = code;
    info->level = level;
    info->module = module;
    info->message = message ? strdup(message) : NULL;
    info->file = file ? strdup(file) : NULL;
    info->line = line;
    info->timestamp = (uint64_t)time(NULL);
    info->next = NULL;
    
    return info;
}

// 销毁错误信息
static void destroy_error_info(error_info *info) {
    if (info) {
        if (info->message) {
            free(info->message);
        }
        if (info->file) {
            free(info->file);
        }
        free(info);
    }
}

// 记录错误
error_code error_record(error_system *error_sys, error_module module, error_level level, error_code code, const char *file, int line, const char *format, ...) {
    if (!error_sys || !error_sys->initialized) {
        return ERROR_INVALID_PARAMETER;
    }
    
    // 生成错误消息
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // 创建错误信息
    error_info *info = create_error_info(module, level, code, file, line, message);
    if (!info) {
        return ERROR_OUT_OF_MEMORY;
    }
    
    // 添加到错误队列
    if (error_sys->error_count >= error_sys->max_errors) {
        // 队列已满，移除最早的错误
        error_info *oldest = error_sys->error_queue;
        if (oldest) {
            error_sys->error_queue = oldest->next;
            destroy_error_info(oldest);
            error_sys->error_count--;
        }
    }
    
    // 添加到队列末尾
    if (!error_sys->error_queue) {
        error_sys->error_queue = info;
    } else {
        error_info *current = error_sys->error_queue;
        while (current->next) {
            current = current->next;
        }
        current->next = info;
    }
    
    error_sys->error_count++;
    return code;
}

// 获取最后一个错误
error_info *error_get_last(error_system *error_sys) {
    if (!error_sys || !error_sys->initialized || error_sys->error_count == 0) {
        return NULL;
    }
    
    error_info *current = error_sys->error_queue;
    while (current->next) {
        current = current->next;
    }
    
    return current;
}

// 获取错误队列
error_info *error_get_queue(error_system *error_sys) {
    if (!error_sys || !error_sys->initialized) {
        return NULL;
    }
    
    return error_sys->error_queue;
}

// 清空错误队列
bool error_clear_queue(error_system *error_sys) {
    if (!error_sys || !error_sys->initialized) {
        return false;
    }
    
    error_info *current = error_sys->error_queue;
    while (current) {
        error_info *next = current->next;
        destroy_error_info(current);
        current = next;
    }
    
    error_sys->error_queue = NULL;
    error_sys->error_count = 0;
    return true;
}

// 获取错误描述
const char *error_get_description(error_code code) {
    for (int i = 0; error_descriptions[i].description; i++) {
        if (error_descriptions[i].code == code) {
            return error_descriptions[i].description;
        }
    }
    return "Unknown error";
}

// 检查错误是否为致命错误
bool error_is_fatal(error_code code) {
    for (int i = 0; fatal_errors[i] != 0; i++) {
        if (fatal_errors[i] == code) {
            return true;
        }
    }
    return false;
}

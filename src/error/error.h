#ifndef ERROR_H
#define ERROR_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

// 错误级别
typedef enum {
    ERROR_LEVEL_INFO,
    ERROR_LEVEL_WARNING,
    ERROR_LEVEL_ERROR,
    ERROR_LEVEL_FATAL
} error_level;

// 错误模块
typedef enum {
    ERROR_MODULE_GENERAL,
    ERROR_MODULE_MEMORY,
    ERROR_MODULE_STORAGE,
    ERROR_MODULE_INDEX,
    ERROR_MODULE_SECURITY,
    ERROR_MODULE_NETWORK,
    ERROR_MODULE_CONFIG,
    ERROR_MODULE_TRANSACTION,
    ERROR_MODULE_RESOURCE,
    ERROR_MODULE_OPTIMIZER,
    ERROR_MODULE_PROCEDURE,
    ERROR_MODULE_REPLICATION,
    ERROR_MODULE_CLIENT
} error_module;

// 错误码定义
typedef enum {
    // 通用错误
    ERROR_SUCCESS = 0,
    ERROR_UNKNOWN = 1,
    ERROR_OUT_OF_MEMORY = 2,
    ERROR_INVALID_PARAMETER = 3,
    ERROR_NOT_FOUND = 4,
    ERROR_ALREADY_EXISTS = 5,
    ERROR_PERMISSION_DENIED = 6,
    ERROR_OPERATION_FAILED = 7,
    ERROR_TIMEOUT = 8,
    
    // 内存错误
    ERROR_MEMORY_ALLOCATION_FAILED = 100,
    ERROR_MEMORY_POOL_FULL = 101,
    ERROR_MEMORY_CORRUPTION = 102,
    
    // 存储错误
    ERROR_STORAGE_IO_FAILED = 200,
    ERROR_STORAGE_DISK_FULL = 201,
    ERROR_STORAGE_FILE_CORRUPT = 202,
    ERROR_STORAGE_LOCK_FAILED = 203,
    
    // 索引错误
    ERROR_INDEX_CREATION_FAILED = 300,
    ERROR_INDEX_CORRUPT = 301,
    ERROR_INDEX_NOT_FOUND = 302,
    
    // 安全错误
    ERROR_SECURITY_AUTH_FAILED = 400,
    ERROR_SECURITY_INVALID_CREDENTIALS = 401,
    ERROR_SECURITY_SSL_ERROR = 402,
    ERROR_SECURITY_PERMISSION_DENIED = 403,
    
    // 网络错误
    ERROR_NETWORK_CONNECTION_FAILED = 500,
    ERROR_NETWORK_SOCKET_ERROR = 501,
    ERROR_NETWORK_TIMEOUT = 502,
    ERROR_NETWORK_PACKET_CORRUPT = 503,
    
    // 配置错误
    ERROR_CONFIG_PARSE_FAILED = 600,
    ERROR_CONFIG_INVALID_VALUE = 601,
    ERROR_CONFIG_MISSING_REQUIRED = 602,
    
    // 事务错误
    ERROR_TRANSACTION_BEGIN_FAILED = 700,
    ERROR_TRANSACTION_COMMIT_FAILED = 701,
    ERROR_TRANSACTION_ROLLBACK_FAILED = 702,
    ERROR_TRANSACTION_DEADLOCK = 703,
    ERROR_TRANSACTION_TIMEOUT = 704,
    
    // 资源错误
    ERROR_RESOURCE_LIMIT_EXCEEDED = 800,
    ERROR_RESOURCE_INSUFFICIENT = 801,
    ERROR_RESOURCE_ALLOCATION_FAILED = 802,
    
    // 优化器错误
    ERROR_OPTIMIZER_PARSE_FAILED = 900,
    ERROR_OPTIMIZER_PLAN_FAILED = 901,
    ERROR_OPTIMIZER_EXECUTE_FAILED = 902,
    ERROR_OPTIMIZER_STATISTICS_FAILED = 903,
    
    // 存储过程和触发器错误
    ERROR_PROCEDURE_CREATE_FAILED = 1000,
    ERROR_PROCEDURE_EXECUTE_FAILED = 1001,
    ERROR_PROCEDURE_DROP_FAILED = 1002,
    ERROR_TRIGGER_CREATE_FAILED = 1003,
    ERROR_TRIGGER_FIRE_FAILED = 1004,
    ERROR_TRIGGER_DROP_FAILED = 1005,
    
    // 复制错误
    ERROR_REPLICATION_CONNECT_FAILED = 1100,
    ERROR_REPLICATION_SYNC_FAILED = 1101,
    ERROR_REPLICATION_BINLOG_FAILED = 1102,
    ERROR_REPLICATION_ROLE_CONFLICT = 1103,
    ERROR_REPLICATION_SLAVE_LIMIT = 1104,
    
    // 客户端错误
    ERROR_CLIENT_CONNECT_FAILED = 1200,
    ERROR_CLIENT_EXECUTE_FAILED = 1201,
    ERROR_CLIENT_PARSE_FAILED = 1202,
    ERROR_CLIENT_COMMAND_NOT_FOUND = 1203,
    ERROR_CLIENT_CONNECTION_LIMIT = 1204,
    
    // 测试错误
    ERROR_SKIP = 1300,
    ERROR_FAIL = 1301
} error_code;

// 错误信息结构
typedef struct {
    error_code code;
    error_level level;
    error_module module;
    char *message;
    char *file;
    int line;
    uint64_t timestamp;
    struct error_info *next;
} error_info;

// 错误处理系统结构
typedef struct {
    error_info *error_queue;
    uint32_t error_count;
    uint32_t max_errors;
    bool initialized;
} error_system;

// 初始化错误处理系统
error_system *error_init(uint32_t max_errors);

// 销毁错误处理系统
void error_destroy(error_system *error_sys);

// 记录错误
error_code error_record(error_system *error_sys, error_module module, error_level level, error_code code, const char *file, int line, const char *format, ...);

// 获取最后一个错误
error_info *error_get_last(error_system *error_sys);

// 获取错误队列
error_info *error_get_queue(error_system *error_sys);

// 清空错误队列
bool error_clear_queue(error_system *error_sys);

// 获取错误描述
const char *error_get_description(error_code code);

// 检查错误是否为致命错误
bool error_is_fatal(error_code code);

// 错误处理宏
#define ERROR_RECORD(error_sys, module, level, code, format, ...) \
    error_record(error_sys, module, level, code, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define ERROR_INFO(error_sys, module, code, format, ...) \
    error_record(error_sys, module, ERROR_LEVEL_INFO, code, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define ERROR_WARNING(error_sys, module, code, format, ...) \
    error_record(error_sys, module, ERROR_LEVEL_WARNING, code, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define ERROR_ERROR(error_sys, module, code, format, ...) \
    error_record(error_sys, module, ERROR_LEVEL_ERROR, code, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define ERROR_FATAL(error_sys, module, code, format, ...) \
    error_record(error_sys, module, ERROR_LEVEL_FATAL, code, __FILE__, __LINE__, format, ##__VA_ARGS__)

#endif // ERROR_H

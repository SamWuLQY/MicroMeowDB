#ifndef AUDIT_H
#define AUDIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 审计事件类型
#define AUDIT_EVENT_LOGIN 0           // 登录
#define AUDIT_EVENT_LOGOUT 1          // 注销
#define AUDIT_EVENT_QUERY 2           // 查询
#define AUDIT_EVENT_INSERT 3          // 插入
#define AUDIT_EVENT_UPDATE 4          // 更新
#define AUDIT_EVENT_DELETE 5          // 删除
#define AUDIT_EVENT_CREATE_TABLE 6    // 创建表
#define AUDIT_EVENT_DROP_TABLE 7      // 删除表
#define AUDIT_EVENT_ALTER_TABLE 8     // 修改表
#define AUDIT_EVENT_CREATE_USER 9     // 创建用户
#define AUDIT_EVENT_DROP_USER 10      // 删除用户
#define AUDIT_EVENT_ALTER_USER 11     // 修改用户
#define AUDIT_EVENT_GRANT 12          // 授权
#define AUDIT_EVENT_REVOKE 13         // 撤销授权
#define AUDIT_EVENT_BACKUP 14         // 备份
#define AUDIT_EVENT_RESTORE 15        // 恢复
#define AUDIT_EVENT_STARTUP 16        // 启动
#define AUDIT_EVENT_SHUTDOWN 17       // 关闭
#define AUDIT_EVENT_ERROR 18          // 错误
#define AUDIT_EVENT_WARNING 19        // 警告
#define AUDIT_EVENT_ADMIN 20          // 管理操作

// 审计事件状态
#define AUDIT_STATUS_SUCCESS 0        // 成功
#define AUDIT_STATUS_FAILURE 1        // 失败

// 审计日志格式
#define AUDIT_FORMAT_TEXT 0           // 文本格式
#define AUDIT_FORMAT_JSON 1           // JSON格式
#define AUDIT_FORMAT_BINARY 2         // 二进制格式

// 审计事件结构
typedef struct {
    uint64_t timestamp;     // 时间戳
    int event_type;         // 事件类型
    int status;             // 事件状态
    char* user;             // 用户名
    char* host;             // 主机名
    char* ip;               // IP地址
    char* database;         // 数据库名
    char* object;           // 对象名
    char* statement;        // SQL语句
    char* error_message;    // 错误信息
    char* details;          // 详细信息
} AuditEvent;

// 审计配置结构
typedef struct {
    bool enabled;           // 是否启用
    char* log_dir;          // 日志目录
    char* log_file;         // 日志文件
    int log_format;         // 日志格式
    int max_log_size;       // 最大日志大小（MB）
    int max_log_files;      // 最大日志文件数
    bool rotate;            // 是否轮转
    bool compress;          // 是否压缩
    bool encrypt;           // 是否加密
    char* encryption_key;   // 加密密钥
    bool log_login;         // 记录登录
    bool log_logout;        // 记录注销
    bool log_query;         // 记录查询
    bool log_dml;           // 记录DML操作
    bool log_ddl;           // 记录DDL操作
    bool log_admin;         // 记录管理操作
    bool log_error;         // 记录错误
    int min_query_length;   // 最小查询长度
    int max_query_length;   // 最大查询长度
} AuditConfig;

// 审计管理器结构
typedef struct {
    AuditConfig* config;    // 审计配置
    FILE* log_file;         // 日志文件
    uint64_t log_size;      // 日志大小
    char* current_log;      // 当前日志文件
} AuditManager;

// 初始化审计管理器
AuditManager* audit_manager_init(AuditConfig* config);

// 销毁审计管理器
void audit_manager_destroy(AuditManager* manager);

// 记录审计事件
bool audit_log_event(AuditManager* manager, AuditEvent* event);

// 记录登录事件
bool audit_log_login(AuditManager* manager, const char* user, const char* host, const char* ip, bool success, const char* error_message);

// 记录注销事件
bool audit_log_logout(AuditManager* manager, const char* user, const char* host, const char* ip);

// 记录查询事件
bool audit_log_query(AuditManager* manager, const char* user, const char* host, const char* ip, const char* database, const char* statement, bool success, const char* error_message);

// 记录DML事件
bool audit_log_dml(AuditManager* manager, int event_type, const char* user, const char* host, const char* ip, const char* database, const char* object, const char* statement, bool success, const char* error_message);

// 记录DDL事件
bool audit_log_ddl(AuditManager* manager, int event_type, const char* user, const char* host, const char* ip, const char* database, const char* object, const char* statement, bool success, const char* error_message);

// 记录管理事件
bool audit_log_admin(AuditManager* manager, int event_type, const char* user, const char* host, const char* ip, const char* details, bool success, const char* error_message);

// 记录错误事件
bool audit_log_error(AuditManager* manager, const char* user, const char* host, const char* ip, const char* error_message, const char* details);

// 记录警告事件
bool audit_log_warning(AuditManager* manager, const char* user, const char* host, const char* ip, const char* warning_message, const char* details);

// 轮转日志文件
bool audit_rotate_log(AuditManager* manager);

// 刷新日志
bool audit_flush_log(AuditManager* manager);

// 设置审计配置
bool audit_set_config(AuditManager* manager, AuditConfig* config);

// 获取审计配置
AuditConfig* audit_get_config(AuditManager* manager);

// 检查审计是否启用
bool audit_is_enabled(AuditManager* manager);

// 启用审计
bool audit_enable(AuditManager* manager);

// 禁用审计
bool audit_disable(AuditManager* manager);

#endif // AUDIT_H
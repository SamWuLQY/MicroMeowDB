#ifndef BACKUP_H
#define BACKUP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 备份类型
#define BACKUP_TYPE_FULL 0      // 完整备份
#define BACKUP_TYPE_INCREMENTAL 1  // 增量备份

// 备份状态
#define BACKUP_STATUS_PENDING 0  // 待处理
#define BACKUP_STATUS_RUNNING 1  // 运行中
#define BACKUP_STATUS_COMPLETED 2  // 已完成
#define BACKUP_STATUS_FAILED 3   // 失败

// 备份文件结构
typedef struct {
    char* path;           // 备份文件路径
    char* name;           // 备份文件名称
    int type;             // 备份类型
    uint64_t timestamp;   // 备份时间戳
    uint64_t size;        // 备份文件大小
    char* status;         // 备份状态
    char* error_message;  // 错误信息（如果有）
    char* parent_backup;  // 父备份（增量备份时）
} BackupFile;

// 备份配置结构
typedef struct {
    char* backup_dir;     // 备份目录
    int max_backups;      // 最大备份文件数
    bool compress;        // 是否压缩
    char* compression_level;  // 压缩级别
    bool encrypt;         // 是否加密
    char* encryption_key; // 加密密钥
    int backup_type;      // 默认备份类型
    char* schedule;       // 备份计划
} BackupConfig;

// 备份管理器结构
typedef struct {
    BackupConfig* config;  // 备份配置
    BackupFile** backups;  // 备份文件列表
    size_t backup_count;   // 备份文件数量
} BackupManager;

// 初始化备份管理器
BackupManager* backup_manager_init(BackupConfig* config);

// 销毁备份管理器
void backup_manager_destroy(BackupManager* manager);

// 执行备份
bool backup_perform(BackupManager* manager, int type, const char* name);

// 执行恢复
bool backup_restore(BackupManager* manager, const char* backup_name);

// 列出所有备份
BackupFile** backup_list(BackupManager* manager, size_t* count);

// 删除备份
bool backup_delete(BackupManager* manager, const char* backup_name);

// 验证备份
bool backup_verify(BackupManager* manager, const char* backup_name);

// 设置备份配置
bool backup_set_config(BackupManager* manager, BackupConfig* config);

// 获取备份配置
BackupConfig* backup_get_config(BackupManager* manager);

// 估算备份大小
bool backup_estimate_size(BackupManager* manager, int type, uint64_t* size);

// 备份进度回调函数类型
typedef void (*backup_progress_callback)(const char* operation, const char* status, uint64_t bytes_processed, uint64_t total_bytes);

// 设置备份进度回调
void backup_set_progress_callback(backup_progress_callback callback);

#endif // BACKUP_H
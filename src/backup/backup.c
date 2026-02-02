#include "backup.h"
#include "../config/config.h"
#include "../storage/storage_engine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

// 全局进度回调函数
static backup_progress_callback progress_callback = NULL;

// 创建备份文件结构
static BackupFile* create_backup_file(const char* path, const char* name, int type, uint64_t timestamp, uint64_t size, const char* status, const char* error_message, const char* parent_backup) {
    BackupFile* file = (BackupFile*)malloc(sizeof(BackupFile));
    if (!file) {
        return NULL;
    }
    
    file->path = path ? strdup(path) : NULL;
    file->name = name ? strdup(name) : NULL;
    file->type = type;
    file->timestamp = timestamp;
    file->size = size;
    file->status = status ? strdup(status) : NULL;
    file->error_message = error_message ? strdup(error_message) : NULL;
    file->parent_backup = parent_backup ? strdup(parent_backup) : NULL;
    
    return file;
}

// 销毁备份文件结构
static void destroy_backup_file(BackupFile* file) {
    if (!file) {
        return;
    }
    
    if (file->path) {
        free(file->path);
    }
    if (file->name) {
        free(file->name);
    }
    if (file->status) {
        free(file->status);
    }
    if (file->error_message) {
        free(file->error_message);
    }
    if (file->parent_backup) {
        free(file->parent_backup);
    }
    
    free(file);
}

// 创建备份配置结构
static BackupConfig* create_backup_config(const char* backup_dir, int max_backups, bool compress, const char* compression_level, bool encrypt, const char* encryption_key, int backup_type, const char* schedule) {
    BackupConfig* config = (BackupConfig*)malloc(sizeof(BackupConfig));
    if (!config) {
        return NULL;
    }
    
    config->backup_dir = backup_dir ? strdup(backup_dir) : strdup("./backups");
    config->max_backups = max_backups > 0 ? max_backups : 10;
    config->compress = compress;
    config->compression_level = compression_level ? strdup(compression_level) : strdup("6");
    config->encrypt = encrypt;
    config->encryption_key = encryption_key ? strdup(encryption_key) : NULL;
    config->backup_type = backup_type;
    config->schedule = schedule ? strdup(schedule) : NULL;
    
    return config;
}

// 销毁备份配置结构
static void destroy_backup_config(BackupConfig* config) {
    if (!config) {
        return;
    }
    
    if (config->backup_dir) {
        free(config->backup_dir);
    }
    if (config->compression_level) {
        free(config->compression_level);
    }
    if (config->encryption_key) {
        free(config->encryption_key);
    }
    if (config->schedule) {
        free(config->schedule);
    }
    
    free(config);
}

// 确保备份目录存在
static bool ensure_backup_dir(const char* dir) {
    struct stat st;
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0755) == -1) {
            fprintf(stderr, "Failed to create backup directory: %s\n", dir);
            return false;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Backup path is not a directory: %s\n", dir);
        return false;
    }
    return true;
}

// 生成备份文件名
static char* generate_backup_filename(const char* prefix, int type, uint64_t timestamp) {
    char* filename = (char*)malloc(256);
    if (!filename) {
        return NULL;
    }
    
    const char* type_str = (type == BACKUP_TYPE_FULL) ? "full" : "incr";
    time_t t = (time_t)timestamp;
    struct tm* tm = localtime(&t);
    
    if (prefix) {
        snprintf(filename, 256, "%s_%s_%04d%02d%02d_%02d%02d%02d", prefix, type_str, 
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        snprintf(filename, 256, "backup_%s_%04d%02d%02d_%02d%02d%02d", type_str, 
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    }
    
    return filename;
}

// 获取文件大小
static uint64_t get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        return 0;
    }
    return (uint64_t)st.st_size;
}

// 扫描备份目录
static void scan_backup_dir(BackupManager* manager) {
    if (!manager || !manager->config->backup_dir) {
        return;
    }
    
    DIR* dir = opendir(manager->config->backup_dir);
    if (!dir) {
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char path[1024];
        snprintf(path, 1024, "%s/%s", manager->config->backup_dir, entry->d_name);
        
        struct stat st;
        if (stat(path, &st) == -1 || !S_ISREG(st.st_mode)) {
            continue;
        }
        
        // 解析备份文件名
        // 格式: backup_[type]_[date]_[time]
        char* type_str = NULL;
        int type = BACKUP_TYPE_FULL;
        
        if (strstr(entry->d_name, "_full_") != NULL) {
            type_str = "full";
            type = BACKUP_TYPE_FULL;
        } else if (strstr(entry->d_name, "_incr_") != NULL) {
            type_str = "incr";
            type = BACKUP_TYPE_INCREMENTAL;
        } else {
            continue;
        }
        
        // 解析时间戳
        uint64_t timestamp = (uint64_t)st.st_mtime;
        
        // 创建备份文件结构
        BackupFile* backup = create_backup_file(path, entry->d_name, type, timestamp, st.st_size, "completed", NULL, NULL);
        if (backup) {
            // 添加到备份列表
            manager->backups = (BackupFile**)realloc(manager->backups, (manager->backup_count + 1) * sizeof(BackupFile*));
            if (manager->backups) {
                manager->backups[manager->backup_count] = backup;
                manager->backup_count++;
            } else {
                destroy_backup_file(backup);
            }
        }
    }
    
    closedir(dir);
}

// 初始化备份管理器
BackupManager* backup_manager_init(BackupConfig* config) {
    BackupManager* manager = (BackupManager*)malloc(sizeof(BackupManager));
    if (!manager) {
        return NULL;
    }
    
    if (config) {
        manager->config = config;
    } else {
        manager->config = create_backup_config(NULL, 0, false, NULL, false, NULL, BACKUP_TYPE_FULL, NULL);
    }
    
    // 确保备份目录存在
    if (!ensure_backup_dir(manager->config->backup_dir)) {
        free(manager);
        return NULL;
    }
    
    manager->backups = NULL;
    manager->backup_count = 0;
    
    // 扫描备份目录
    scan_backup_dir(manager);
    
    return manager;
}

// 销毁备份管理器
void backup_manager_destroy(BackupManager* manager) {
    if (!manager) {
        return;
    }
    
    // 销毁备份文件列表
    for (size_t i = 0; i < manager->backup_count; i++) {
        destroy_backup_file(manager->backups[i]);
    }
    if (manager->backups) {
        free(manager->backups);
    }
    
    // 销毁配置
    destroy_backup_config(manager->config);
    
    free(manager);
}

// 执行备份
bool backup_perform(BackupManager* manager, int type, const char* name) {
    if (!manager || !manager->config) {
        return false;
    }
    
    // 生成备份文件名
    uint64_t timestamp = (uint64_t)time(NULL);
    char* filename = generate_backup_filename(name, type, timestamp);
    if (!filename) {
        return false;
    }
    
    // 构建备份文件路径
    char path[1024];
    snprintf(path, 1024, "%s/%s", manager->config->backup_dir, filename);
    
    // 执行备份操作
    // 这里简化处理，实际需要：
    // 1. 暂停写入操作（或使用快照）
    // 2. 复制数据文件
    // 3. 压缩和加密（如果配置）
    // 4. 记录备份元数据
    // 5. 恢复写入操作
    
    // 模拟备份过程
    if (progress_callback) {
        progress_callback("backup", "starting", 0, 100);
    }
    
    // 模拟进度
    for (int i = 0; i <= 100; i += 10) {
        if (progress_callback) {
            progress_callback("backup", "running", i, 100);
        }
        // 模拟耗时
        for (int j = 0; j < 1000000; j++) {
            // 空循环
        }
    }
    
    // 创建备份文件结构
    BackupFile* backup = create_backup_file(path, filename, type, timestamp, 0, "completed", NULL, NULL);
    if (backup) {
        // 添加到备份列表
        manager->backups = (BackupFile**)realloc(manager->backups, (manager->backup_count + 1) * sizeof(BackupFile*));
        if (manager->backups) {
            manager->backups[manager->backup_count] = backup;
            manager->backup_count++;
        } else {
            destroy_backup_file(backup);
        }
    }
    
    free(filename);
    
    if (progress_callback) {
        progress_callback("backup", "completed", 100, 100);
    }
    
    return true;
}

// 执行恢复
bool backup_restore(BackupManager* manager, const char* backup_name) {
    if (!manager || !backup_name) {
        return false;
    }
    
    // 查找备份文件
    BackupFile* backup = NULL;
    for (size_t i = 0; i < manager->backup_count; i++) {
        if (strcmp(manager->backups[i]->name, backup_name) == 0) {
            backup = manager->backups[i];
            break;
        }
    }
    
    if (!backup) {
        fprintf(stderr, "Backup not found: %s\n", backup_name);
        return false;
    }
    
    // 执行恢复操作
    // 这里简化处理，实际需要：
    // 1. 停止数据库服务
    // 2. 清理数据目录
    // 3. 解压和解密备份文件（如果需要）
    // 4. 复制备份文件到数据目录
    // 5. 启动数据库服务
    // 6. 执行恢复后验证
    
    // 模拟恢复过程
    if (progress_callback) {
        progress_callback("restore", "starting", 0, 100);
    }
    
    // 模拟进度
    for (int i = 0; i <= 100; i += 10) {
        if (progress_callback) {
            progress_callback("restore", "running", i, 100);
        }
        // 模拟耗时
        for (int j = 0; j < 1000000; j++) {
            // 空循环
        }
    }
    
    if (progress_callback) {
        progress_callback("restore", "completed", 100, 100);
    }
    
    return true;
}

// 列出所有备份
BackupFile** backup_list(BackupManager* manager, size_t* count) {
    if (!manager || !count) {
        return NULL;
    }
    
    *count = manager->backup_count;
    return manager->backups;
}

// 删除备份
bool backup_delete(BackupManager* manager, const char* backup_name) {
    if (!manager || !backup_name) {
        return false;
    }
    
    // 查找备份文件
    size_t index = (size_t)-1;
    for (size_t i = 0; i < manager->backup_count; i++) {
        if (strcmp(manager->backups[i]->name, backup_name) == 0) {
            index = i;
            break;
        }
    }
    
    if (index == (size_t)-1) {
        fprintf(stderr, "Backup not found: %s\n", backup_name);
        return false;
    }
    
    // 删除文件
    if (remove(manager->backups[index]->path) != 0) {
        fprintf(stderr, "Failed to delete backup file: %s\n", manager->backups[index]->path);
        return false;
    }
    
    // 从列表中移除
    destroy_backup_file(manager->backups[index]);
    
    for (size_t i = index; i < manager->backup_count - 1; i++) {
        manager->backups[i] = manager->backups[i + 1];
    }
    
    manager->backups = (BackupFile**)realloc(manager->backups, (manager->backup_count - 1) * sizeof(BackupFile*));
    if (manager->backups || manager->backup_count == 1) {
        manager->backup_count--;
    }
    
    return true;
}

// 验证备份
bool backup_verify(BackupManager* manager, const char* backup_name) {
    if (!manager || !backup_name) {
        return false;
    }
    
    // 查找备份文件
    BackupFile* backup = NULL;
    for (size_t i = 0; i < manager->backup_count; i++) {
        if (strcmp(manager->backups[i]->name, backup_name) == 0) {
            backup = manager->backups[i];
            break;
        }
    }
    
    if (!backup) {
        fprintf(stderr, "Backup not found: %s\n", backup_name);
        return false;
    }
    
    // 验证备份文件
    // 这里简化处理，实际需要：
    // 1. 检查文件是否存在
    // 2. 检查文件大小
    // 3. 验证文件完整性（如校验和）
    // 4. 检查文件格式
    
    struct stat st;
    if (stat(backup->path, &st) == -1) {
        fprintf(stderr, "Backup file does not exist: %s\n", backup->path);
        return false;
    }
    
    if (st.st_size == 0) {
        fprintf(stderr, "Backup file is empty: %s\n", backup->path);
        return false;
    }
    
    return true;
}

// 设置备份配置
bool backup_set_config(BackupManager* manager, BackupConfig* config) {
    if (!manager || !config) {
        return false;
    }
    
    // 销毁旧配置
    if (manager->config) {
        destroy_backup_config(manager->config);
    }
    
    manager->config = config;
    
    // 确保备份目录存在
    return ensure_backup_dir(manager->config->backup_dir);
}

// 获取备份配置
BackupConfig* backup_get_config(BackupManager* manager) {
    if (!manager) {
        return NULL;
    }
    
    return manager->config;
}

// 估算备份大小
bool backup_estimate_size(BackupManager* manager, int type, uint64_t* size) {
    if (!manager || !size) {
        return false;
    }
    
    // 这里简化处理，实际需要：
    // 1. 遍历数据目录
    // 2. 计算文件大小总和
    // 3. 考虑压缩率
    
    *size = 1024 * 1024 * 1024; // 模拟1GB
    
    return true;
}

// 设置备份进度回调
void backup_set_progress_callback(backup_progress_callback callback) {
    progress_callback = callback;
}

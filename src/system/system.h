#ifndef MICROMEOW_SYSTEM_H
#define MICROMEOW_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

// 系统状态
typedef enum {
    MM_SYSTEM_STATE_UNINITIALIZED,
    MM_SYSTEM_STATE_INITIALIZING,
    MM_SYSTEM_STATE_RUNNING,
    MM_SYSTEM_STATE_SHUTTING_DOWN,
    MM_SYSTEM_STATE_SHUTDOWN
} mm_system_state;

// 子系统类型
typedef enum {
    MM_SUBSYSTEM_CONFIG,
    MM_SUBSYSTEM_ERROR,
    MM_SUBSYSTEM_LOGGING,
    MM_SUBSYSTEM_MEMORY,
    MM_SUBSYSTEM_RESOURCE,
    MM_SUBSYSTEM_STORAGE,
    MM_SUBSYSTEM_METADATA,
    MM_SUBSYSTEM_INDEX,
    MM_SUBSYSTEM_OPTIMIZER,
    MM_SUBSYSTEM_PROCEDURE,
    MM_SUBSYSTEM_SECURITY,
    MM_SUBSYSTEM_TRANSACTION,
    MM_SUBSYSTEM_NETWORK,
    MM_SUBSYSTEM_REPLICATION,
    MM_SUBSYSTEM_MONITORING,
    MM_SUBSYSTEM_AUDIT,
    MM_SUBSYSTEM_BACKUP,
    MM_SUBSYSTEM_MAX
} mm_subsystem_type;

// 子系统结构
typedef struct {
    mm_subsystem_type type;
    const char *name;
    bool initialized;
    bool started;
    void *instance;
} mm_subsystem;

// 系统配置结构
typedef struct {
    char *config_file;
    char *data_dir;
    char *log_dir;
    bool daemonize;
    char *pid_file;
} mm_system_config;

// 系统结构
typedef struct {
    mm_system_state state;
    mm_subsystem subsystems[MM_SUBSYSTEM_MAX];
    uint64_t start_time;
    uint64_t shutdown_time;
    bool initialized;
    const mm_system_config *config;
} mm_system_manager;

// 初始化系统管理器
mm_system_manager *mm_system_init(const mm_system_config *config);

// 销毁系统管理器
void mm_system_destroy(mm_system_manager *system_mgr);

// 启动系统
bool mm_system_start(mm_system_manager *system_mgr);

// 关闭系统
bool mm_system_shutdown(mm_system_manager *system_mgr);

// 获取系统状态
mm_system_state mm_system_get_state(mm_system_manager *system_mgr);

// 获取子系统状态
bool mm_system_get_subsystem_state(mm_system_manager *system_mgr, mm_subsystem_type type, bool *initialized, bool *started);

// 注册子系统
bool mm_system_register_subsystem(mm_system_manager *system_mgr, mm_subsystem_type type, const char *name, void *instance);

// 初始化子系统
bool mm_system_init_subsystem(mm_system_manager *system_mgr, mm_subsystem_type type);

// 启动子系统
bool mm_system_start_subsystem(mm_system_manager *system_mgr, mm_subsystem_type type);

// 停止子系统
bool mm_system_stop_subsystem(mm_system_manager *system_mgr, mm_subsystem_type type);

// 检查系统健康状态
bool mm_system_check_health(mm_system_manager *system_mgr);

// 获取系统运行时间
uint64_t mm_system_get_uptime(mm_system_manager *system_mgr);

// 处理系统信号
void mm_system_handle_signal(int signal);

#endif // MICROMEOW_SYSTEM_H

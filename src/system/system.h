#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

// 系统状态
typedef enum {
    SYSTEM_STATE_UNINITIALIZED,
    SYSTEM_STATE_INITIALIZING,
    SYSTEM_STATE_RUNNING,
    SYSTEM_STATE_SHUTTING_DOWN,
    SYSTEM_STATE_SHUTDOWN
} system_state;

// 子系统类型
typedef enum {
    SUBSYSTEM_CONFIG,
    SUBSYSTEM_ERROR,
    SUBSYSTEM_LOGGING,
    SUBSYSTEM_MEMORY,
    SUBSYSTEM_RESOURCE,
    SUBSYSTEM_STORAGE,
    SUBSYSTEM_METADATA,
    SUBSYSTEM_INDEX,
    SUBSYSTEM_OPTIMIZER,
    SUBSYSTEM_PROCEDURE,
    SUBSYSTEM_SECURITY,
    SUBSYSTEM_TRANSACTION,
    SUBSYSTEM_NETWORK,
    SUBSYSTEM_REPLICATION,
    SUBSYSTEM_MONITORING,
    SUBSYSTEM_AUDIT,
    SUBSYSTEM_BACKUP,
    SUBSYSTEM_MAX
} subsystem_type;

// 子系统结构
typedef struct {
    subsystem_type type;
    const char *name;
    bool initialized;
    bool started;
    void *instance;
} subsystem;

// 系统结构
typedef struct {
    system_state state;
    subsystem subsystems[SUBSYSTEM_MAX];
    uint64_t start_time;
    uint64_t shutdown_time;
    bool initialized;
    const system_config *config;
} system_manager;

// 系统配置结构
typedef struct {
    char *config_file;
    char *data_dir;
    char *log_dir;
    bool daemonize;
    char *pid_file;
} system_config;

// 初始化系统管理器
system_manager *system_init(const system_config *config);

// 销毁系统管理器
void system_destroy(system_manager *system);

// 启动系统
bool system_start(system_manager *system);

// 关闭系统
bool system_shutdown(system_manager *system);

// 获取系统状态
system_state system_get_state(system_manager *system);

// 获取子系统状态
bool system_get_subsystem_state(system_manager *system, subsystem_type type, bool *initialized, bool *started);

// 注册子系统
bool system_register_subsystem(system_manager *system, subsystem_type type, const char *name, void *instance);

// 初始化子系统
bool system_init_subsystem(system_manager *system, subsystem_type type);

// 启动子系统
bool system_start_subsystem(system_manager *system, subsystem_type type);

// 停止子系统
bool system_stop_subsystem(system_manager *system, subsystem_type type);

// 检查系统健康状态
bool system_check_health(system_manager *system);

// 获取系统运行时间
uint64_t system_get_uptime(system_manager *system);

// 处理系统信号
void system_handle_signal(int signal);

#endif // SYSTEM_H

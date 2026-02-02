#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

// 平台检测和兼容性处理
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #define getpid _getpid
    #define pause() Sleep(1000)  // Windows下的暂停实现
    
    // 定义Linux信号常量
    #ifndef SIGHUP
        #define SIGHUP 1
    #endif
    #ifndef SIGUSR1
        #define SIGUSR1 2
    #endif
    #ifndef SIGUSR2
        #define SIGUSR2 3
    #endif
    #ifndef SIGTERM
        #define SIGTERM 15
    #endif
    #ifndef SIGINT
        #define SIGINT 2
    #endif
#else
    #include <unistd.h>
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/wait.h>
#endif

// 子系统类型枚举 - 必须与system.c中的subsystem_names数组一致
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

// 前置声明
struct config_system;
struct MetadataManager;
struct MemoryPool;
struct StorageEngineManager;
struct AuditConfig;
struct BackupConfig;

// 系统状态枚举
typedef enum {
    SYSTEM_STATE_UNINITIALIZED,
    SYSTEM_STATE_INITIALIZING,
    SYSTEM_STATE_RUNNING,
    SYSTEM_STATE_SHUTTING_DOWN,
    SYSTEM_STATE_SHUTDOWN
} system_state;

// 子系统结构
typedef struct {
    subsystem_type type;
    const char *name;
    bool initialized;
    bool started;
    void *instance;
} subsystem;

// 系统配置结构
typedef struct {
    char *config_file;
    char *data_dir;
    char *log_dir;
    bool daemonize;
    char *pid_file;
} system_config;

// 系统管理器结构
typedef struct {
    system_state state;
    subsystem subsystems[SUBSYSTEM_MAX];
    uint64_t start_time;
    uint64_t shutdown_time;
    bool initialized;
    const system_config *config;
    
    // 平台特定字段
    #ifdef _WIN32
        HANDLE process_handle;
        DWORD windows_process_id;
    #else
        pid_t process_id;
    #endif
} system_manager;

// 全局系统管理器实例 - 修改名称避免冲突
extern system_manager *g_system_manager;

// 函数声明
#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_H

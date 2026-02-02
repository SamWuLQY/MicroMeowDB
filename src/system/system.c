#include "system.h"
#include "../config/config.h"
#include "../resource/resource.h"
#include "../optimizer/optimizer.h"
#include "../procedure/procedure.h"
#include "../replication/replication.h"
#include "../storage/storage_engine.h"
#include "../memory/memory_pool.h"
#include "../memory/memory_cache.h"
#include "../backup/backup.h"
#include "../metadata/metadata.h"
#include "../util/path.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Windows 兼容性定义
#ifdef _WIN32
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGUSR1
#define SIGUSR1 2
#endif
#ifndef SIGUSR2
#define SIGUSR2 3
#endif
#ifndef SIGINT
#define SIGINT 2
#endif
#ifndef SIGTERM
#define SIGTERM 15
#endif
#ifndef getpid
#define getpid() 0
#endif
#endif

// 子系统名称映射
static const char *subsystem_names[] = {
    "Config",
    "Error",
    "Logging", 
    "Memory",
    "Resource",
    "Storage",
    "Metadata",
    "Index",
    "Optimizer",
    "Procedure",
    "Security",
    "Transaction",
    "Network",
    "Replication",
    "Monitoring",
    "Audit",
    "Backup"
};

// 全局系统管理器实例
static mm_system_manager *global_mm_system = NULL;

// 初始化系统管理器
mm_system_manager *mm_system_init(const mm_system_config *config) {
    mm_system_manager *system_mgr = (mm_system_manager *)malloc(sizeof(mm_system_manager));
    if (!system_mgr) {
        return NULL;
    }
    
    // 初始化系统状态
    system_mgr->state = MM_SYSTEM_STATE_UNINITIALIZED;
    system_mgr->start_time = 0;
    system_mgr->shutdown_time = 0;
    system_mgr->initialized = false;
    system_mgr->config = config;
    
    // 初始化子系统
    for (int i = 0; i < MM_SUBSYSTEM_MAX; i++) {
        system_mgr->subsystems[i].type = (mm_subsystem_type)i;
        system_mgr->subsystems[i].name = subsystem_names[i];
        system_mgr->subsystems[i].initialized = false;
        system_mgr->subsystems[i].started = false;
        system_mgr->subsystems[i].instance = NULL;
    }
    
    system_mgr->initialized = true;
    global_mm_system = system_mgr;
    
    return system_mgr;
}

// 销毁系统管理器
void mm_system_destroy(mm_system_manager *system_mgr) {
    if (!system_mgr) {
        return;
    }
    
    // 确保系统已关闭
    if (system_mgr->state == MM_SYSTEM_STATE_RUNNING) {
        mm_system_shutdown(system_mgr);
    }
    
    system_mgr->initialized = false;
    
    if (global_mm_system == system_mgr) {
        global_mm_system = NULL;
    }
    
    free(system_mgr);
}

// 启动系统
bool mm_system_start(mm_system_manager *system_mgr) {
    if (!system_mgr || !system_mgr->initialized) {
        return false;
    }
    
    if (system_mgr->state != MM_SYSTEM_STATE_UNINITIALIZED) {
        return false;
    }
    
    system_mgr->state = MM_SYSTEM_STATE_INITIALIZING;
    
    // 初始化子系统（按依赖顺序）
    mm_subsystem_type init_order[] = {
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
        MM_SUBSYSTEM_BACKUP
    };
    
    for (int i = 0; i < MM_SUBSYSTEM_MAX; i++) {
        mm_subsystem_type type = init_order[i];
        if (!mm_system_init_subsystem(system_mgr, type)) {
            system_mgr->state = MM_SYSTEM_STATE_SHUTDOWN;
            return false;
        }
    }
    
    // 启动子系统
    for (int i = 0; i < MM_SUBSYSTEM_MAX; i++) {
        mm_subsystem_type type = init_order[i];
        if (!mm_system_start_subsystem(system_mgr, type)) {
            system_mgr->state = MM_SYSTEM_STATE_SHUTDOWN;
            return false;
        }
    }
    
    system_mgr->state = MM_SYSTEM_STATE_RUNNING;
    system_mgr->start_time = (uint64_t)time(NULL);
    
    return true;
}

// 关闭系统
bool mm_system_shutdown(mm_system_manager *system_mgr) {
    if (!system_mgr || !system_mgr->initialized) {
        return false;
    }
    
    if (system_mgr->state != MM_SYSTEM_STATE_RUNNING) {
        return false;
    }
    
    system_mgr->state = MM_SYSTEM_STATE_SHUTTING_DOWN;
    
    // 停止子系统（按相反依赖顺序）
    mm_subsystem_type stop_order[] = {
        MM_SUBSYSTEM_BACKUP,
        MM_SUBSYSTEM_AUDIT,
        MM_SUBSYSTEM_MONITORING,
        MM_SUBSYSTEM_REPLICATION,
        MM_SUBSYSTEM_NETWORK,
        MM_SUBSYSTEM_TRANSACTION,
        MM_SUBSYSTEM_SECURITY,
        MM_SUBSYSTEM_PROCEDURE,
        MM_SUBSYSTEM_OPTIMIZER,
        MM_SUBSYSTEM_INDEX,
        MM_SUBSYSTEM_METADATA,
        MM_SUBSYSTEM_STORAGE,
        MM_SUBSYSTEM_RESOURCE,
        MM_SUBSYSTEM_MEMORY,
        MM_SUBSYSTEM_LOGGING,
        MM_SUBSYSTEM_ERROR,
        MM_SUBSYSTEM_CONFIG
    };
    
    for (int i = 0; i < MM_SUBSYSTEM_MAX; i++) {
        mm_subsystem_type type = stop_order[i];
        mm_system_stop_subsystem(system_mgr, type);
    }
    
    system_mgr->state = MM_SYSTEM_STATE_SHUTDOWN;
    system_mgr->shutdown_time = (uint64_t)time(NULL);
    
    return true;
}

// 获取系统状态
mm_system_state mm_system_get_state(mm_system_manager *system_mgr) {
    if (!system_mgr || !system_mgr->initialized) {
        return MM_SYSTEM_STATE_UNINITIALIZED;
    }
    return system_mgr->state;
}

// 获取子系统状态
bool mm_system_get_subsystem_state(mm_system_manager *system_mgr, mm_subsystem_type type, bool *initialized, bool *started) {
    if (!system_mgr || !system_mgr->initialized || type >= MM_SUBSYSTEM_MAX) {
        return false;
    }
    
    if (initialized) {
        *initialized = system_mgr->subsystems[type].initialized;
    }
    if (started) {
        *started = system_mgr->subsystems[type].started;
    }
    
    return true;
}

// 注册子系统
bool mm_system_register_subsystem(mm_system_manager *system_mgr, mm_subsystem_type type, const char *name, void *instance) {
    if (!system_mgr || !system_mgr->initialized || type >= MM_SUBSYSTEM_MAX) {
        return false;
    }
    
    system_mgr->subsystems[type].name = name ? name : subsystem_names[type];
    system_mgr->subsystems[type].instance = instance;
    
    return true;
}

// 初始化子系统
bool mm_system_init_subsystem(mm_system_manager *system_mgr, mm_subsystem_type type) {
    if (!system_mgr || !system_mgr->initialized || type >= MM_SUBSYSTEM_MAX) {
        return false;
    }
    
    mm_subsystem *subsys = &system_mgr->subsystems[type];
    if (subsys->initialized) {
        return true;
    }
    
    // 子系统初始化逻辑
    switch (type) {
        case MM_SUBSYSTEM_CONFIG:
            {
                // 初始化配置系统
                mm_system_config *sys_config = (mm_system_config *)system_mgr->config;
                config_system *config = config_init(sys_config->config_file);
                if (config) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_CONFIG, "Config", config);
                }
            }
            break;
        case MM_SUBSYSTEM_ERROR:
            {
                // 初始化错误处理系统
                void *error_manager = error_init();
                if (error_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_ERROR, "Error", error_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_LOGGING:
            {
                // 初始化日志系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                void *logging_manager = logging_init(config);
                if (logging_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_LOGGING, "Logging", logging_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_MEMORY:
            {
                // 初始化内存池
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                MemoryPool *pool = memory_pool_init(config);
                if (pool) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_MEMORY, "Memory", pool);
                }
            }
            break;
        case MM_SUBSYSTEM_RESOURCE:
            {
                // 初始化资源管理子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                void *resource_manager = resource_manager_create(config);
                if (resource_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_RESOURCE, "Resource", resource_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_STORAGE:
            {
                // 初始化存储引擎
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                StorageEngineManager *storage = storage_engine_manager_init(config);
                if (storage) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_STORAGE, "Storage", storage);
                }
            }
            break;
        case MM_SUBSYSTEM_METADATA:
            {
                // 初始化元数据子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                const char* metadata_dir = config_get_string(config, "metadata.metadata_dir", "./metadata");
                MetadataManager *metadata_manager = metadata_manager_init(metadata_dir);
                if (metadata_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_METADATA, "Metadata", metadata_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_INDEX:
            {
                // 初始化索引子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                void *index_manager = index_manager_init(config);
                if (index_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_INDEX, "Index", index_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_OPTIMIZER:
            {
                // 初始化查询优化器子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                MetadataManager *metadata = (MetadataManager *)system_mgr->subsystems[MM_SUBSYSTEM_METADATA].instance;
                void *optimizer = optimizer_create(config, metadata);
                if (optimizer) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_OPTIMIZER, "Optimizer", optimizer);
                }
            }
            break;
        case MM_SUBSYSTEM_PROCEDURE:
            {
                // 初始化存储过程和触发器子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                MetadataManager *metadata = (MetadataManager *)system_mgr->subsystems[MM_SUBSYSTEM_METADATA].instance;
                void *procedure_manager = procedure_manager_create(config, metadata);
                if (procedure_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_PROCEDURE, "Procedure", procedure_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_SECURITY:
            {
                // 初始化安全子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                void *security_manager = security_init(config);
                if (security_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_SECURITY, "Security", security_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_TRANSACTION:
            {
                // 初始化事务子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                void *transaction_manager = transaction_manager_init(config);
                if (transaction_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_TRANSACTION, "Transaction", transaction_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_NETWORK:
            {
                // 初始化网络子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                void *network_server = network_server_init(config);
                if (network_server) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_NETWORK, "Network", network_server);
                }
            }
            break;
        case MM_SUBSYSTEM_REPLICATION:
            {
                // 初始化复制子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                void *network_server = system_mgr->subsystems[MM_SUBSYSTEM_NETWORK].instance;
                void *replication_manager = replication_manager_create(config, network_server);
                if (replication_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_REPLICATION, "Replication", replication_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_MONITORING:
            {
                // 初始化监控子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                void *monitoring_manager = monitoring_init(config);
                if (monitoring_manager) {
                    mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_MONITORING, "Monitoring", monitoring_manager);
                }
            }
            break;
        case MM_SUBSYSTEM_AUDIT:
            {
                // 初始化审计子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                AuditConfig *audit_config = (AuditConfig *)malloc(sizeof(AuditConfig));
                if (audit_config) {
                    audit_config->enabled = config_get_bool(config, "audit.enabled", true);
                    audit_config->log_dir = config_get_string(config, "audit.log_dir", "./audit");
                    audit_config->log_file = config_get_string(config, "audit.log_file", "audit");
                    audit_config->log_format = config_get_int(config, "audit.log_format", AUDIT_FORMAT_TEXT);
                    audit_config->max_log_size = config_get_int(config, "audit.max_log_size", 100);
                    audit_config->max_log_files = config_get_int(config, "audit.max_log_files", 10);
                    audit_config->rotate = config_get_bool(config, "audit.rotate", true);
                    audit_config->compress = config_get_bool(config, "audit.compress", false);
                    audit_config->encrypt = config_get_bool(config, "audit.encrypt", false);
                    audit_config->encryption_key = config_get_string(config, "audit.encryption_key", NULL);
                    audit_config->log_login = config_get_bool(config, "audit.log_login", true);
                    audit_config->log_logout = config_get_bool(config, "audit.log_logout", true);
                    audit_config->log_query = config_get_bool(config, "audit.log_query", true);
                    audit_config->log_dml = config_get_bool(config, "audit.log_dml", true);
                    audit_config->log_ddl = config_get_bool(config, "audit.log_ddl", true);
                    audit_config->log_admin = config_get_bool(config, "audit.log_admin", true);
                    audit_config->log_error = config_get_bool(config, "audit.log_error", true);
                    audit_config->min_query_length = config_get_int(config, "audit.min_query_length", 0);
                    audit_config->max_query_length = config_get_int(config, "audit.max_query_length", 10240);
                    
                    AuditManager *audit_manager = audit_manager_init(audit_config);
                    if (audit_manager) {
                        mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_AUDIT, "Audit", audit_manager);
                    } else {
                        free(audit_config);
                    }
                }
            }
            break;
        case MM_SUBSYSTEM_BACKUP:
            {
                // 初始化备份子系统
                config_system *config = (config_system *)system_mgr->subsystems[MM_SUBSYSTEM_CONFIG].instance;
                BackupConfig *backup_config = (BackupConfig *)malloc(sizeof(BackupConfig));
                if (backup_config) {
                    backup_config->backup_dir = config_get_string(config, "backup.backup_dir", "./backups");
                    backup_config->max_backups = config_get_int(config, "backup.max_backups", 10);
                    backup_config->compress = config_get_bool(config, "backup.compress", false);
                    backup_config->compression_level = config_get_string(config, "backup.compression_level", "6");
                    backup_config->encrypt = config_get_bool(config, "backup.encrypt", false);
                    backup_config->encryption_key = config_get_string(config, "backup.encryption_key", NULL);
                    backup_config->backup_type = config_get_int(config, "backup.backup_type", BACKUP_TYPE_FULL);
                    backup_config->schedule = config_get_string(config, "backup.schedule", NULL);
                    
                    BackupManager *backup_manager = backup_manager_init(backup_config);
                    if (backup_manager) {
                        mm_system_register_subsystem(system_mgr, MM_SUBSYSTEM_BACKUP, "Backup", backup_manager);
                    } else {
                        free(backup_config);
                    }
                }
            }
            break;
        default:
            // 其他子系统的初始化逻辑
            break;
    }
    
    subsys->initialized = true;
    
    return true;
}

// 启动子系统
bool mm_system_start_subsystem(mm_system_manager *system_mgr, mm_subsystem_type type) {
    if (!system_mgr || !system_mgr->initialized || type >= MM_SUBSYSTEM_MAX) {
        return false;
    }
    
    mm_subsystem *subsys = &system_mgr->subsystems[type];
    if (!subsys->initialized) {
        return false;
    }
    
    if (subsys->started) {
        return true;
    }
    
    // 子系统启动逻辑将由各个模块实现
    // 这里只是标记为启动
    subsys->started = true;
    
    return true;
}

// 停止子系统
bool mm_system_stop_subsystem(mm_system_manager *system_mgr, mm_subsystem_type type) {
    if (!system_mgr || !system_mgr->initialized || type >= MM_SUBSYSTEM_MAX) {
        return false;
    }
    
    mm_subsystem *subsys = &system_mgr->subsystems[type];
    if (!subsys->started) {
        return true;
    }
    
    // 子系统停止逻辑将由各个模块实现
    // 这里只是标记为停止
    subsys->started = false;
    
    return true;
}

// 检查系统健康状态
bool mm_system_check_health(mm_system_manager *system_mgr) {
    if (!system_mgr || !system_mgr->initialized) {
        return false;
    }
    
    if (system_mgr->state != MM_SYSTEM_STATE_RUNNING) {
        return false;
    }
    
    // 检查所有子系统是否正常运行
    for (int i = 0; i < MM_SUBSYSTEM_MAX; i++) {
        mm_subsystem *subsys = &system_mgr->subsystems[i];
        if (subsys->initialized && !subsys->started) {
            return false;
        }
    }
    
    return true;
}

// 获取系统运行时间
uint64_t mm_system_get_uptime(mm_system_manager *system_mgr) {
    if (!system_mgr || !system_mgr->initialized || system_mgr->state != MM_SYSTEM_STATE_RUNNING) {
        return 0;
    }
    
    return (uint64_t)time(NULL) - system_mgr->start_time;
}

// 处理系统信号
void mm_system_handle_signal(int signal) {
    switch (signal) {
        case SIGINT:
        case SIGTERM:
            if (global_mm_system) {
                mm_system_shutdown(global_mm_system);
            }
            break;
        case SIGHUP:
            // 处理配置重新加载
            break;
        case SIGUSR1:
            // 处理用户自定义信号1
            break;
        case SIGUSR2:
            // 处理用户自定义信号2
            break;
    }
}

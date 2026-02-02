#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 查找配置项
static int find_config_item(config_system *config, const char *key) {
    for (uint32_t i = 0; i < config->count; i++) {
        if (strcmp(config->items[i]->key, key) == 0) {
            return i;
        }
    }
    return -1;
}

// 创建配置项
static config_item *create_config_item(const char *key, config_type type, const char *description) {
    config_item *item = (config_item *)malloc(sizeof(config_item));
    if (!item) {
        return NULL;
    }
    
    item->key = strdup(key);
    if (!item->key) {
        free(item);
        return NULL;
    }
    
    item->type = type;
    item->description = description ? strdup(description) : NULL;
    item->is_default = true;
    
    return item;
}

// 销毁配置项
static void destroy_config_item(config_item *item) {
    if (item) {
        if (item->key) {
            free(item->key);
        }
        if (item->type == CONFIG_TYPE_STRING && item->value.string_val) {
            free(item->value.string_val);
        }
        if (item->description) {
            free(item->description);
        }
        free(item);
    }
}

// 扩展配置系统容量
static bool expand_config_system(config_system *config) {
    uint32_t new_capacity = config->capacity * 2;
    config_item **new_items = (config_item **)realloc(
        config->items, new_capacity * sizeof(config_item *)
    );
    if (!new_items) {
        return false;
    }
    
    config->items = new_items;
    config->capacity = new_capacity;
    return true;
}

// 解析配置文件行
static bool parse_config_line(const char *line, char **key, char **value) {
    // 跳过空白字符
    while (*line && isspace(*line)) {
        line++;
    }
    
    // 跳过注释行
    if (*line == '#' || *line == ';') {
        return false;
    }
    
    // 查找键值对分隔符
    const char *sep = strchr(line, '=');
    if (!sep) {
        return false;
    }
    
    // 提取键
    size_t key_len = sep - line;
    *key = (char *)malloc(key_len + 1);
    if (!*key) {
        return false;
    }
    strncpy(*key, line, key_len);
    (*key)[key_len] = '\0';
    
    // 跳过键后的空白
    while (key_len > 0 && isspace((*key)[key_len - 1])) {
        key_len--;
        (*key)[key_len] = '\0';
    }
    
    // 提取值
    line = sep + 1;
    while (*line && isspace(*line)) {
        line++;
    }
    
    size_t value_len = strlen(line);
    while (value_len > 0 && isspace(line[value_len - 1])) {
        value_len--;
    }
    
    *value = (char *)malloc(value_len + 1);
    if (!*value) {
        free(*key);
        return false;
    }
    strncpy(*value, line, value_len);
    (*value)[value_len] = '\0';
    
    return true;
}

// 初始化配置系统
config_system *config_init(const char *config_file) {
    config_system *config = (config_system *)malloc(sizeof(config_system));
    if (!config) {
        return NULL;
    }
    
    config->capacity = 64;
    config->items = (config_item **)calloc(config->capacity, sizeof(config_item *));
    if (!config->items) {
        free(config);
        return NULL;
    }
    
    config->count = 0;
    config->config_file = config_file ? strdup(config_file) : NULL;
    config->loaded = false;
    
    // 注册默认配置
    config_register_defaults(config);
    
    // 尝试加载配置文件
    if (config_file) {
        config_load(config);
    }
    
    return config;
}

// 销毁配置系统
void config_destroy(config_system *config) {
    if (config) {
        for (uint32_t i = 0; i < config->count; i++) {
            destroy_config_item(config->items[i]);
        }
        if (config->items) {
            free(config->items);
        }
        if (config->config_file) {
            free(config->config_file);
        }
        free(config);
    }
}

// 加载配置文件
bool config_load(config_system *config) {
    if (!config || !config->config_file) {
        return false;
    }
    
    FILE *fp = fopen(config->config_file, "r");
    if (!fp) {
        return false;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *key = NULL;
        char *value = NULL;
        
        if (!parse_config_line(line, &key, &value)) {
            continue;
        }
        
        // 根据值的格式判断类型并设置
        if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) {
            bool bool_val = (strcmp(value, "true") == 0);
            config_set_bool(config, key, bool_val, NULL);
        } else if (strchr(value, '.') != NULL) {
            double double_val = atof(value);
            config_set_double(config, key, double_val, NULL);
        } else {
            char *endptr;
            long int_val = strtol(value, &endptr, 10);
            if (*endptr == '\0') {
                config_set_int(config, key, (int32_t)int_val, NULL);
            } else {
                config_set_string(config, key, value, NULL);
            }
        }
        
        free(key);
        free(value);
    }
    
    fclose(fp);
    config->loaded = true;
    return true;
}

// 保存配置文件
bool config_save(config_system *config) {
    if (!config || !config->config_file) {
        return false;
    }
    
    FILE *fp = fopen(config->config_file, "w");
    if (!fp) {
        return false;
    }
    
    fprintf(fp, "# MicroMeowDB Configuration File\n");
    fprintf(fp, "# Generated on %s\n\n", __DATE__);
    
    for (uint32_t i = 0; i < config->count; i++) {
        config_item *item = config->items[i];
        if (item->description) {
            fprintf(fp, "# %s\n", item->description);
        }
        
        switch (item->type) {
            case CONFIG_TYPE_INT:
                fprintf(fp, "%s=%d\n", item->key, item->value.int_val);
                break;
            case CONFIG_TYPE_BOOL:
                fprintf(fp, "%s=%s\n", item->key, item->value.bool_val ? "true" : "false");
                break;
            case CONFIG_TYPE_STRING:
                fprintf(fp, "%s=%s\n", item->key, item->value.string_val);
                break;
            case CONFIG_TYPE_DOUBLE:
                fprintf(fp, "%s=%f\n", item->key, item->value.double_val);
                break;
        }
        
        if (i < config->count - 1) {
            fprintf(fp, "\n");
        }
    }
    
    fclose(fp);
    return true;
}

// 获取整数配置
int32_t config_get_int(config_system *config, const char *key, int32_t default_val) {
    int index = find_config_item(config, key);
    if (index >= 0 && config->items[index]->type == CONFIG_TYPE_INT) {
        return config->items[index]->value.int_val;
    }
    return default_val;
}

// 获取布尔配置
bool config_get_bool(config_system *config, const char *key, bool default_val) {
    int index = find_config_item(config, key);
    if (index >= 0 && config->items[index]->type == CONFIG_TYPE_BOOL) {
        return config->items[index]->value.bool_val;
    }
    return default_val;
}

// 获取字符串配置
const char *config_get_string(config_system *config, const char *key, const char *default_val) {
    int index = find_config_item(config, key);
    if (index >= 0 && config->items[index]->type == CONFIG_TYPE_STRING) {
        return config->items[index]->value.string_val;
    }
    return default_val;
}

// 获取浮点数配置
double config_get_double(config_system *config, const char *key, double default_val) {
    int index = find_config_item(config, key);
    if (index >= 0 && config->items[index]->type == CONFIG_TYPE_DOUBLE) {
        return config->items[index]->value.double_val;
    }
    return default_val;
}

// 设置整数配置
bool config_set_int(config_system *config, const char *key, int32_t value, const char *description) {
    int index = find_config_item(config, key);
    if (index >= 0) {
        config_item *item = config->items[index];
        if (item->type != CONFIG_TYPE_INT) {
            // 类型不匹配，更新类型
            if (item->type == CONFIG_TYPE_STRING && item->value.string_val) {
                free(item->value.string_val);
            }
            item->type = CONFIG_TYPE_INT;
        }
        item->value.int_val = value;
        item->is_default = false;
        if (description && !item->description) {
            item->description = strdup(description);
        }
    } else {
        // 创建新配置项
        if (config->count >= config->capacity) {
            if (!expand_config_system(config)) {
                return false;
            }
        }
        
        config_item *item = create_config_item(key, CONFIG_TYPE_INT, description);
        if (!item) {
            return false;
        }
        item->value.int_val = value;
        item->is_default = false;
        config->items[config->count++] = item;
    }
    return true;
}

// 设置布尔配置
bool config_set_bool(config_system *config, const char *key, bool value, const char *description) {
    int index = find_config_item(config, key);
    if (index >= 0) {
        config_item *item = config->items[index];
        if (item->type != CONFIG_TYPE_BOOL) {
            // 类型不匹配，更新类型
            if (item->type == CONFIG_TYPE_STRING && item->value.string_val) {
                free(item->value.string_val);
            }
            item->type = CONFIG_TYPE_BOOL;
        }
        item->value.bool_val = value;
        item->is_default = false;
        if (description && !item->description) {
            item->description = strdup(description);
        }
    } else {
        // 创建新配置项
        if (config->count >= config->capacity) {
            if (!expand_config_system(config)) {
                return false;
            }
        }
        
        config_item *item = create_config_item(key, CONFIG_TYPE_BOOL, description);
        if (!item) {
            return false;
        }
        item->value.bool_val = value;
        item->is_default = false;
        config->items[config->count++] = item;
    }
    return true;
}

// 设置字符串配置
bool config_set_string(config_system *config, const char *key, const char *value, const char *description) {
    int index = find_config_item(config, key);
    if (index >= 0) {
        config_item *item = config->items[index];
        if (item->type != CONFIG_TYPE_STRING) {
            // 类型不匹配，更新类型
            item->type = CONFIG_TYPE_STRING;
        } else if (item->value.string_val) {
            free(item->value.string_val);
        }
        item->value.string_val = strdup(value);
        item->is_default = false;
        if (description && !item->description) {
            item->description = strdup(description);
        }
    } else {
        // 创建新配置项
        if (config->count >= config->capacity) {
            if (!expand_config_system(config)) {
                return false;
            }
        }
        
        config_item *item = create_config_item(key, CONFIG_TYPE_STRING, description);
        if (!item) {
            return false;
        }
        item->value.string_val = strdup(value);
        item->is_default = false;
        config->items[config->count++] = item;
    }
    return true;
}

// 设置浮点数配置
bool config_set_double(config_system *config, const char *key, double value, const char *description) {
    int index = find_config_item(config, key);
    if (index >= 0) {
        config_item *item = config->items[index];
        if (item->type != CONFIG_TYPE_DOUBLE) {
            // 类型不匹配，更新类型
            if (item->type == CONFIG_TYPE_STRING && item->value.string_val) {
                free(item->value.string_val);
            }
            item->type = CONFIG_TYPE_DOUBLE;
        }
        item->value.double_val = value;
        item->is_default = false;
        if (description && !item->description) {
            item->description = strdup(description);
        }
    } else {
        // 创建新配置项
        if (config->count >= config->capacity) {
            if (!expand_config_system(config)) {
                return false;
            }
        }
        
        config_item *item = create_config_item(key, CONFIG_TYPE_DOUBLE, description);
        if (!item) {
            return false;
        }
        item->value.double_val = value;
        item->is_default = false;
        config->items[config->count++] = item;
    }
    return true;
}

// 注册默认配置
void config_register_defaults(config_system *config) {
    // 通用配置
    config_set_int(config, "general.port", 3306, "Database server port");
    config_set_string(config, "general.bind_address", "127.0.0.1", "Bind address for server");
    config_set_int(config, "general.max_connections", 1000, "Maximum number of concurrent connections");
    config_set_int(config, "general.connection_timeout", 300, "Connection timeout in seconds");
    config_set_int(config, "general.thread_pool_size", 8, "Thread pool size for handling requests");
    config_set_int(config, "general.max_packet_size", 1048576, "Maximum packet size in bytes");
    config_set_bool(config, "general.enable_query_cache", true, "Enable query cache");
    config_set_int(config, "general.query_cache_size", 64, "Query cache size in MB");
    
    // 安全配置
    config_set_bool(config, "security.ssl_enabled", false, "Enable SSL encryption");
    config_set_string(config, "security.ssl_cert", "server.crt", "SSL certificate file");
    config_set_string(config, "security.ssl_key", "server.key", "SSL private key file");
    config_set_string(config, "security.ssl_ca", "ca.crt", "SSL CA certificate file");
    config_set_bool(config, "security.password_validation", true, "Enable password validation");
    config_set_int(config, "security.min_password_length", 8, "Minimum password length");
    config_set_int(config, "security.password_expire_days", 90, "Password expiration days");
    config_set_int(config, "security.failed_login_attempts", 5, "Maximum failed login attempts");
    config_set_int(config, "security.lockout_duration", 300, "Account lockout duration in seconds");
    config_set_bool(config, "security.enable_audit", true, "Enable audit logging");
    
    // 存储配置
    config_set_string(config, "storage.data_dir", "./data", "Data directory");
    config_set_int(config, "storage.buffer_pool_size", 1024, "Buffer pool size in MB");
    config_set_int(config, "storage.max_open_files", 1024, "Maximum number of open files");
    config_set_bool(config, "storage.sync_binlog", true, "Sync binlog to disk");
    config_set_int(config, "storage.binlog_cache_size", 32, "Binlog cache size in MB");
    config_set_string(config, "storage.binlog_format", "ROW", "Binlog format (STATEMENT, ROW, MIXED)");
    config_set_int(config, "storage.innodb_buffer_pool_instances", 8, "Number of InnoDB buffer pool instances");
    config_set_int(config, "storage.innodb_log_file_size", 256, "InnoDB log file size in MB");
    config_set_int(config, "storage.innodb_log_files_in_group", 2, "Number of InnoDB log files in group");
    config_set_int(config, "storage.innodb_flush_log_at_trx_commit", 1, "InnoDB flush log at transaction commit");
    
    // 内存配置
    config_set_int(config, "memory.memory_pool_size", 512, "Memory pool size in MB");
    config_set_int(config, "memory.cache_size", 256, "Cache size in MB");
    config_set_double(config, "memory.cache_eviction_threshold", 0.8, "Cache eviction threshold");
    config_set_int(config, "memory.max_heap_table_size", 64, "Maximum heap table size in MB");
    config_set_int(config, "memory.tmp_table_size", 64, "Temporary table size in MB");
    
    // 索引配置
    config_set_int(config, "index.b_plus_tree_order", 32, "B+ tree order");
    config_set_int(config, "index.lsm_memtable_size", 10, "LSM tree memtable size in MB");
    config_set_int(config, "index.lsm_sstable_size", 64, "LSM tree SSTable size in MB");
    config_set_int(config, "index.lsm_compaction_threads", 4, "LSM tree compaction threads");
    config_set_int(config, "index.hash_bucket_size", 1024, "Hash index bucket size");
    config_set_int(config, "index.r_tree_min_entries", 2, "R tree minimum entries per node");
    config_set_int(config, "index.r_tree_max_entries", 8, "R tree maximum entries per node");
    config_set_int(config, "index.bloom_filter_size", 1024, "Bloom filter size in bytes");
    config_set_int(config, "index.bloom_filter_hash_functions", 4, "Bloom filter hash functions count");
    
    // 网络配置
    config_set_int(config, "network.socket_timeout", 300, "Socket timeout in seconds");
    config_set_int(config, "network.connect_timeout", 10, "Connect timeout in seconds");
    config_set_int(config, "network.read_timeout", 30, "Read timeout in seconds");
    config_set_int(config, "network.write_timeout", 30, "Write timeout in seconds");
    config_set_int(config, "network.backlog", 128, "Network backlog size");
    
    // 日志配置
    config_set_string(config, "logging.log_level", "info", "Log level (debug, info, warn, error)");
    config_set_string(config, "logging.log_file", "micromeowdb.log", "Log file path");
    config_set_bool(config, "logging.log_rotation", true, "Enable log rotation");
    config_set_int(config, "logging.log_max_size", 100, "Maximum log file size in MB");
    config_set_int(config, "logging.log_max_files", 10, "Maximum number of log files");
    config_set_bool(config, "logging.log_compress", false, "Enable log compression");
    config_set_string(config, "logging.log_format", "text", "Log format (text, json)");
    
    // 备份配置
    config_set_string(config, "backup.backup_dir", "./backups", "Backup directory");
    config_set_int(config, "backup.max_backups", 10, "Maximum number of backups to keep");
    config_set_bool(config, "backup.compress", false, "Enable backup compression");
    config_set_string(config, "backup.compression_level", "6", "Compression level (1-9)");
    config_set_bool(config, "backup.encrypt", false, "Enable backup encryption");
    config_set_string(config, "backup.encryption_key", NULL, "Backup encryption key");
    config_set_int(config, "backup.backup_type", 0, "Default backup type (0: full, 1: incremental)");
    config_set_string(config, "backup.schedule", NULL, "Backup schedule (cron format)");
    
    // 审计配置
    config_set_bool(config, "audit.enabled", true, "Enable audit logging");
    config_set_string(config, "audit.log_dir", "./audit", "Audit log directory");
    config_set_string(config, "audit.log_file", "audit", "Audit log file name prefix");
    config_set_int(config, "audit.log_format", 0, "Audit log format (0: text, 1: json)");
    config_set_int(config, "audit.max_log_size", 100, "Maximum audit log size in MB");
    config_set_int(config, "audit.max_log_files", 10, "Maximum number of audit log files");
    config_set_bool(config, "audit.rotate", true, "Enable audit log rotation");
    config_set_bool(config, "audit.compress", false, "Enable audit log compression");
    config_set_bool(config, "audit.encrypt", false, "Enable audit log encryption");
    config_set_string(config, "audit.encryption_key", NULL, "Audit log encryption key");
    config_set_bool(config, "audit.log_login", true, "Log login events");
    config_set_bool(config, "audit.log_logout", true, "Log logout events");
    config_set_bool(config, "audit.log_query", true, "Log query events");
    config_set_bool(config, "audit.log_dml", true, "Log DML events");
    config_set_bool(config, "audit.log_ddl", true, "Log DDL events");
    config_set_bool(config, "audit.log_admin", true, "Log admin events");
    config_set_bool(config, "audit.log_error", true, "Log error events");
    config_set_int(config, "audit.min_query_length", 0, "Minimum query length to log");
    config_set_int(config, "audit.max_query_length", 10240, "Maximum query length to log");
    
    // 资源管理配置
    config_set_int(config, "resource.max_user_connections", 100, "Maximum connections per user");
    config_set_int(config, "resource.max_user_queries", 1000, "Maximum queries per user per hour");
    config_set_int(config, "resource.max_user_updates", 100, "Maximum updates per user per hour");
    config_set_int(config, "resource.max_connection_threads", 100, "Maximum threads per connection");
    config_set_int(config, "resource.max_table_locks", 100, "Maximum table locks per connection");
    config_set_int(config, "resource.max_write_lock_count", 100, "Maximum write locks per connection");
    config_set_int(config, "resource.query_timeout", 300, "Query timeout in seconds");
    config_set_int(config, "resource.lock_wait_timeout", 30, "Lock wait timeout in seconds");
    
    // 复制配置
    config_set_bool(config, "replication.enabled", false, "Enable replication");
    config_set_string(config, "replication.server_id", "1", "Server ID for replication");
    config_set_string(config, "replication.master_host", NULL, "Master host for replication");
    config_set_int(config, "replication.master_port", 3306, "Master port for replication");
    config_set_string(config, "replication.master_user", "replicator", "Master user for replication");
    config_set_string(config, "replication.master_password", NULL, "Master password for replication");
    config_set_string(config, "replication.replicate_do_db", NULL, "Databases to replicate");
    config_set_string(config, "replication.replicate_ignore_db", NULL, "Databases to ignore");
    config_set_int(config, "replication.sync_binlog", 1, "Sync binlog to disk per N transactions");
    config_set_int(config, "replication.slave_net_timeout", 3600, "Slave network timeout in seconds");
    config_set_bool(config, "replication.slave_skip_errors", false, "Skip replication errors");
    
    // 监控配置
    config_set_bool(config, "monitoring.enabled", true, "Enable monitoring");
    config_set_string(config, "monitoring.stats_file", "stats.json", "Statistics file path");
    config_set_int(config, "monitoring.stats_interval", 60, "Statistics collection interval in seconds");
    config_set_bool(config, "monitoring.enable_metrics", true, "Enable metrics collection");
    config_set_bool(config, "monitoring.enable_alerts", false, "Enable alerts");
    config_set_string(config, "monitoring.alert_script", NULL, "Alert script path");
    
    // 元数据配置
    config_set_string(config, "metadata.metadata_dir", "./metadata", "Metadata directory");
    config_set_bool(config, "metadata.enable_cache", true, "Enable metadata cache");
    config_set_int(config, "metadata.cache_size", 64, "Metadata cache size in MB");
    
    // 查询优化器配置
    config_set_bool(config, "optimizer.enable_index_merge", true, "Enable index merge optimization");
    config_set_bool(config, "optimizer.enable_mrr", true, "Enable multi-range read optimization");
    config_set_bool(config, "optimizer.enable_ICP", true, "Enable index condition pushdown");
    config_set_bool(config, "optimizer.enable_hash_join", true, "Enable hash join");
    config_set_bool(config, "optimizer.enable_nested_loop_join", true, "Enable nested loop join");
    config_set_bool(config, "optimizer.enable_sort_merge_join", true, "Enable sort merge join");
    config_set_int(config, "optimizer.max_join_size", 1000000, "Maximum join size");
    config_set_int(config, "optimizer.max_seeks_for_key", 100, "Maximum seeks for key");
    config_set_int(config, "optimizer.join_buffer_size", 16, "Join buffer size in MB");
    config_set_int(config, "optimizer.sort_buffer_size", 16, "Sort buffer size in MB");
    config_set_int(config, "optimizer.read_buffer_size", 1, "Read buffer size in MB");
    config_set_int(config, "optimizer.read_rnd_buffer_size", 1, "Random read buffer size in MB");
    
    // 存储过程配置
    config_set_bool(config, "procedure.enable_procedures", true, "Enable stored procedures");
    config_set_bool(config, "procedure.enable_triggers", true, "Enable triggers");
    config_set_bool(config, "procedure.enable_events", true, "Enable events");
    config_set_int(config, "procedure.max_sp_recursion_depth", 0, "Maximum stored procedure recursion depth");
    config_set_int(config, "procedure.sp_cache_size", 100, "Stored procedure cache size");
}

// 打印配置
void config_print(config_system *config, FILE *stream) {
    if (!config || !stream) {
        return;
    }
    
    fprintf(stream, "MicroMeowDB Configuration\n");
    fprintf(stream, "==============================\n\n");
    
    for (uint32_t i = 0; i < config->count; i++) {
        config_item *item = config->items[i];
        if (item->description) {
            fprintf(stream, "# %s\n", item->description);
        }
        
        switch (item->type) {
            case CONFIG_TYPE_INT:
                fprintf(stream, "%s = %d", item->key, item->value.int_val);
                break;
            case CONFIG_TYPE_BOOL:
                fprintf(stream, "%s = %s", item->key, item->value.bool_val ? "true" : "false");
                break;
            case CONFIG_TYPE_STRING:
                fprintf(stream, "%s = \"%s\"", item->key, item->value.string_val);
                break;
            case CONFIG_TYPE_DOUBLE:
                fprintf(stream, "%s = %f", item->key, item->value.double_val);
                break;
        }
        
        if (item->is_default) {
            fprintf(stream, " (default)");
        }
        fprintf(stream, "\n\n");
    }
}

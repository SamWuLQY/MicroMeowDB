#include "audit.h"
#include "../config/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

// 确保目录存在
static bool ensure_directory(const char* dir) {
    struct stat st;
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0755) == -1) {
            fprintf(stderr, "Failed to create audit directory: %s\n", dir);
            return false;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Audit path is not a directory: %s\n", dir);
        return false;
    }
    return true;
}

// 生成审计日志文件名
static char* generate_audit_filename(const char* dir, const char* base_name, uint64_t timestamp) {
    char* filename = (char*)malloc(512);
    if (!filename) {
        return NULL;
    }
    
    time_t t = (time_t)timestamp;
    struct tm* tm = localtime(&t);
    
    if (tm) {
        snprintf(filename, 512, "%s/%s_%04d%02d%02d_%02d%02d%02d.log", 
                 dir, base_name, 
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        snprintf(filename, 512, "%s/%s_%llu.log", dir, base_name, timestamp);
    }
    
    return filename;
}

// 获取事件类型字符串
static const char* get_event_type_string(int event_type) {
    switch (event_type) {
        case AUDIT_EVENT_LOGIN: return "LOGIN";
        case AUDIT_EVENT_LOGOUT: return "LOGOUT";
        case AUDIT_EVENT_QUERY: return "QUERY";
        case AUDIT_EVENT_INSERT: return "INSERT";
        case AUDIT_EVENT_UPDATE: return "UPDATE";
        case AUDIT_EVENT_DELETE: return "DELETE";
        case AUDIT_EVENT_CREATE_TABLE: return "CREATE_TABLE";
        case AUDIT_EVENT_DROP_TABLE: return "DROP_TABLE";
        case AUDIT_EVENT_ALTER_TABLE: return "ALTER_TABLE";
        case AUDIT_EVENT_CREATE_USER: return "CREATE_USER";
        case AUDIT_EVENT_DROP_USER: return "DROP_USER";
        case AUDIT_EVENT_ALTER_USER: return "ALTER_USER";
        case AUDIT_EVENT_GRANT: return "GRANT";
        case AUDIT_EVENT_REVOKE: return "REVOKE";
        case AUDIT_EVENT_BACKUP: return "BACKUP";
        case AUDIT_EVENT_RESTORE: return "RESTORE";
        case AUDIT_EVENT_STARTUP: return "STARTUP";
        case AUDIT_EVENT_SHUTDOWN: return "SHUTDOWN";
        case AUDIT_EVENT_ERROR: return "ERROR";
        case AUDIT_EVENT_WARNING: return "WARNING";
        case AUDIT_EVENT_ADMIN: return "ADMIN";
        default: return "UNKNOWN";
    }
}

// 获取事件状态字符串
static const char* get_event_status_string(int status) {
    return (status == AUDIT_STATUS_SUCCESS) ? "SUCCESS" : "FAILURE";
}

// 格式化时间戳
static char* format_timestamp(uint64_t timestamp) {
    char* time_str = (char*)malloc(32);
    if (!time_str) {
        return NULL;
    }
    
    time_t t = (time_t)timestamp;
    struct tm* tm = localtime(&t);
    
    if (tm) {
        snprintf(time_str, 32, "%04d-%02d-%02d %02d:%02d:%02d", 
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        snprintf(time_str, 32, "%llu", timestamp);
    }
    
    return time_str;
}

// 写入文本格式日志
static bool write_text_log(FILE* fp, AuditEvent* event) {
    if (!fp || !event) {
        return false;
    }
    
    char* time_str = format_timestamp(event->timestamp);
    if (!time_str) {
        return false;
    }
    
    fprintf(fp, "[%s] [%s] [%s] USER='%s' HOST='%s' IP='%s' DATABASE='%s' OBJECT='%s'\n", 
            time_str, 
            get_event_type_string(event->event_type), 
            get_event_status_string(event->status), 
            event->user ? event->user : "", 
            event->host ? event->host : "", 
            event->ip ? event->ip : "", 
            event->database ? event->database : "", 
            event->object ? event->object : "");
    
    if (event->statement) {
        fprintf(fp, "STATEMENT: %s\n", event->statement);
    }
    
    if (event->error_message) {
        fprintf(fp, "ERROR: %s\n", event->error_message);
    }
    
    if (event->details) {
        fprintf(fp, "DETAILS: %s\n", event->details);
    }
    
    fprintf(fp, "\n");
    free(time_str);
    return true;
}

// 写入JSON格式日志
static bool write_json_log(FILE* fp, AuditEvent* event) {
    if (!fp || !event) {
        return false;
    }
    
    char* time_str = format_timestamp(event->timestamp);
    if (!time_str) {
        return false;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": \"%s\",\n", time_str);
    fprintf(fp, "  \"event_type\": \"%s\",\n", get_event_type_string(event->event_type));
    fprintf(fp, "  \"status\": \"%s\",\n", get_event_status_string(event->status));
    fprintf(fp, "  \"user\": \"%s\",\n", event->user ? event->user : "");
    fprintf(fp, "  \"host\": \"%s\",\n", event->host ? event->host : "");
    fprintf(fp, "  \"ip\": \"%s\",\n", event->ip ? event->ip : "");
    fprintf(fp, "  \"database\": \"%s\",\n", event->database ? event->database : "");
    fprintf(fp, "  \"object\": \"%s\"", event->object ? event->object : "");
    
    if (event->statement) {
        fprintf(fp, ",\n  \"statement\": \"%s\"", event->statement);
    }
    
    if (event->error_message) {
        fprintf(fp, ",\n  \"error_message\": \"%s\"", event->error_message);
    }
    
    if (event->details) {
        fprintf(fp, ",\n  \"details\": \"%s\"", event->details);
    }
    
    fprintf(fp, "\n}\n");
    free(time_str);
    return true;
}

// 检查日志文件大小
static bool check_log_size(AuditManager* manager) {
    if (!manager || !manager->log_file) {
        return false;
    }
    
    if (manager->config->max_log_size > 0) {
        fflush(manager->log_file);
        
        struct stat st;
        if (fstat(fileno(manager->log_file), &st) == 0) {
            uint64_t current_size = (uint64_t)st.st_size;
            if (current_size > (uint64_t)manager->config->max_log_size * 1024 * 1024) {
                return true;
            }
        }
    }
    
    return false;
}

// 打开日志文件
static bool open_log_file(AuditManager* manager) {
    if (!manager || !manager->config) {
        return false;
    }
    
    // 确保日志目录存在
    if (!ensure_directory(manager->config->log_dir)) {
        return false;
    }
    
    // 生成日志文件名
    uint64_t timestamp = (uint64_t)time(NULL);
    char* filename = generate_audit_filename(manager->config->log_dir, manager->config->log_file, timestamp);
    if (!filename) {
        return false;
    }
    
    // 关闭当前日志文件
    if (manager->log_file) {
        fclose(manager->log_file);
        manager->log_file = NULL;
    }
    
    // 打开新日志文件
    manager->log_file = fopen(filename, "a");
    if (!manager->log_file) {
        fprintf(stderr, "Failed to open audit log file: %s\n", filename);
        free(filename);
        return false;
    }
    
    // 更新当前日志信息
    if (manager->current_log) {
        free(manager->current_log);
    }
    manager->current_log = filename;
    manager->log_size = 0;
    
    return true;
}

// 初始化审计管理器
AuditManager* audit_manager_init(AuditConfig* config) {
    AuditManager* manager = (AuditManager*)malloc(sizeof(AuditManager));
    if (!manager) {
        return NULL;
    }
    
    if (config) {
        manager->config = config;
    } else {
        // 创建默认配置
        manager->config = (AuditConfig*)malloc(sizeof(AuditConfig));
        if (!manager->config) {
            free(manager);
            return NULL;
        }
        
        manager->config->enabled = true;
        manager->config->log_dir = strdup("./audit");
        manager->config->log_file = strdup("audit");
        manager->config->log_format = AUDIT_FORMAT_TEXT;
        manager->config->max_log_size = 100;
        manager->config->max_log_files = 10;
        manager->config->rotate = true;
        manager->config->compress = false;
        manager->config->encrypt = false;
        manager->config->encryption_key = NULL;
        manager->config->log_login = true;
        manager->config->log_logout = true;
        manager->config->log_query = true;
        manager->config->log_dml = true;
        manager->config->log_ddl = true;
        manager->config->log_admin = true;
        manager->config->log_error = true;
        manager->config->min_query_length = 0;
        manager->config->max_query_length = 10240;
    }
    
    manager->log_file = NULL;
    manager->log_size = 0;
    manager->current_log = NULL;
    
    // 打开日志文件
    if (!open_log_file(manager)) {
        free(manager->config->log_dir);
        free(manager->config->log_file);
        if (manager->config->encryption_key) {
            free(manager->config->encryption_key);
        }
        free(manager->config);
        free(manager);
        return NULL;
    }
    
    return manager;
}

// 销毁审计管理器
void audit_manager_destroy(AuditManager* manager) {
    if (!manager) {
        return;
    }
    
    // 关闭日志文件
    if (manager->log_file) {
        fclose(manager->log_file);
    }
    
    // 释放当前日志文件名
    if (manager->current_log) {
        free(manager->current_log);
    }
    
    // 释放配置
    if (manager->config) {
        if (manager->config->log_dir) {
            free(manager->config->log_dir);
        }
        if (manager->config->log_file) {
            free(manager->config->log_file);
        }
        if (manager->config->encryption_key) {
            free(manager->config->encryption_key);
        }
        free(manager->config);
    }
    
    free(manager);
}

// 记录审计事件
bool audit_log_event(AuditManager* manager, AuditEvent* event) {
    if (!manager || !event || !manager->config->enabled) {
        return false;
    }
    
    // 检查日志文件大小
    if (manager->config->rotate && check_log_size(manager)) {
        if (!audit_rotate_log(manager)) {
            return false;
        }
    }
    
    // 写入日志
    bool result = false;
    switch (manager->config->log_format) {
        case AUDIT_FORMAT_TEXT:
            result = write_text_log(manager->log_file, event);
            break;
        case AUDIT_FORMAT_JSON:
            result = write_json_log(manager->log_file, event);
            break;
        case AUDIT_FORMAT_BINARY:
            // 二进制格式暂未实现
            result = false;
            break;
        default:
            result = write_text_log(manager->log_file, event);
    }
    
    if (result) {
        fflush(manager->log_file);
    }
    
    return result;
}

// 创建审计事件
static AuditEvent* create_audit_event(int event_type, int status, const char* user, const char* host, const char* ip, const char* database, const char* object, const char* statement, const char* error_message, const char* details) {
    AuditEvent* event = (AuditEvent*)malloc(sizeof(AuditEvent));
    if (!event) {
        return NULL;
    }
    
    event->timestamp = (uint64_t)time(NULL);
    event->event_type = event_type;
    event->status = status;
    event->user = user ? strdup(user) : NULL;
    event->host = host ? strdup(host) : NULL;
    event->ip = ip ? strdup(ip) : NULL;
    event->database = database ? strdup(database) : NULL;
    event->object = object ? strdup(object) : NULL;
    event->statement = statement ? strdup(statement) : NULL;
    event->error_message = error_message ? strdup(error_message) : NULL;
    event->details = details ? strdup(details) : NULL;
    
    return event;
}

// 销毁审计事件
static void destroy_audit_event(AuditEvent* event) {
    if (!event) {
        return;
    }
    
    if (event->user) free(event->user);
    if (event->host) free(event->host);
    if (event->ip) free(event->ip);
    if (event->database) free(event->database);
    if (event->object) free(event->object);
    if (event->statement) free(event->statement);
    if (event->error_message) free(event->error_message);
    if (event->details) free(event->details);
    
    free(event);
}

// 记录登录事件
bool audit_log_login(AuditManager* manager, const char* user, const char* host, const char* ip, bool success, const char* error_message) {
    if (!manager || !manager->config->log_login) {
        return false;
    }
    
    AuditEvent* event = create_audit_event(AUDIT_EVENT_LOGIN, success ? AUDIT_STATUS_SUCCESS : AUDIT_STATUS_FAILURE, user, host, ip, NULL, NULL, NULL, error_message, NULL);
    if (!event) {
        return false;
    }
    
    bool result = audit_log_event(manager, event);
    destroy_audit_event(event);
    return result;
}

// 记录注销事件
bool audit_log_logout(AuditManager* manager, const char* user, const char* host, const char* ip) {
    if (!manager || !manager->config->log_logout) {
        return false;
    }
    
    AuditEvent* event = create_audit_event(AUDIT_EVENT_LOGOUT, AUDIT_STATUS_SUCCESS, user, host, ip, NULL, NULL, NULL, NULL, NULL);
    if (!event) {
        return false;
    }
    
    bool result = audit_log_event(manager, event);
    destroy_audit_event(event);
    return result;
}

// 记录查询事件
bool audit_log_query(AuditManager* manager, const char* user, const char* host, const char* ip, const char* database, const char* statement, bool success, const char* error_message) {
    if (!manager || !manager->config->log_query) {
        return false;
    }
    
    // 检查查询长度
    if (statement) {
        size_t len = strlen(statement);
        if (len < (size_t)manager->config->min_query_length) {
            return false;
        }
        if (len > (size_t)manager->config->max_query_length) {
            // 截断查询
            char* truncated = (char*)malloc((size_t)manager->config->max_query_length + 4);
            if (!truncated) {
                return false;
            }
            strncpy(truncated, statement, (size_t)manager->config->max_query_length);
            truncated[manager->config->max_query_length] = '\0';
            strcat(truncated, "...");
            
            AuditEvent* event = create_audit_event(AUDIT_EVENT_QUERY, success ? AUDIT_STATUS_SUCCESS : AUDIT_STATUS_FAILURE, user, host, ip, database, NULL, truncated, error_message, NULL);
            free(truncated);
            
            if (!event) {
                return false;
            }
            
            bool result = audit_log_event(manager, event);
            destroy_audit_event(event);
            return result;
        }
    }
    
    AuditEvent* event = create_audit_event(AUDIT_EVENT_QUERY, success ? AUDIT_STATUS_SUCCESS : AUDIT_STATUS_FAILURE, user, host, ip, database, NULL, statement, error_message, NULL);
    if (!event) {
        return false;
    }
    
    bool result = audit_log_event(manager, event);
    destroy_audit_event(event);
    return result;
}

// 记录DML事件
bool audit_log_dml(AuditManager* manager, int event_type, const char* user, const char* host, const char* ip, const char* database, const char* object, const char* statement, bool success, const char* error_message) {
    if (!manager || !manager->config->log_dml) {
        return false;
    }
    
    AuditEvent* event = create_audit_event(event_type, success ? AUDIT_STATUS_SUCCESS : AUDIT_STATUS_FAILURE, user, host, ip, database, object, statement, error_message, NULL);
    if (!event) {
        return false;
    }
    
    bool result = audit_log_event(manager, event);
    destroy_audit_event(event);
    return result;
}

// 记录DDL事件
bool audit_log_ddl(AuditManager* manager, int event_type, const char* user, const char* host, const char* ip, const char* database, const char* object, const char* statement, bool success, const char* error_message) {
    if (!manager || !manager->config->log_ddl) {
        return false;
    }
    
    AuditEvent* event = create_audit_event(event_type, success ? AUDIT_STATUS_SUCCESS : AUDIT_STATUS_FAILURE, user, host, ip, database, object, statement, error_message, NULL);
    if (!event) {
        return false;
    }
    
    bool result = audit_log_event(manager, event);
    destroy_audit_event(event);
    return result;
}

// 记录管理事件
bool audit_log_admin(AuditManager* manager, int event_type, const char* user, const char* host, const char* ip, const char* details, bool success, const char* error_message) {
    if (!manager || !manager->config->log_admin) {
        return false;
    }
    
    AuditEvent* event = create_audit_event(event_type, success ? AUDIT_STATUS_SUCCESS : AUDIT_STATUS_FAILURE, user, host, ip, NULL, NULL, NULL, error_message, details);
    if (!event) {
        return false;
    }
    
    bool result = audit_log_event(manager, event);
    destroy_audit_event(event);
    return result;
}

// 记录错误事件
bool audit_log_error(AuditManager* manager, const char* user, const char* host, const char* ip, const char* error_message, const char* details) {
    if (!manager || !manager->config->log_error) {
        return false;
    }
    
    AuditEvent* event = create_audit_event(AUDIT_EVENT_ERROR, AUDIT_STATUS_FAILURE, user, host, ip, NULL, NULL, NULL, error_message, details);
    if (!event) {
        return false;
    }
    
    bool result = audit_log_event(manager, event);
    destroy_audit_event(event);
    return result;
}

// 记录警告事件
bool audit_log_warning(AuditManager* manager, const char* user, const char* host, const char* ip, const char* warning_message, const char* details) {
    if (!manager || !manager->config->log_error) {
        return false;
    }
    
    AuditEvent* event = create_audit_event(AUDIT_EVENT_WARNING, AUDIT_STATUS_FAILURE, user, host, ip, NULL, NULL, NULL, warning_message, details);
    if (!event) {
        return false;
    }
    
    bool result = audit_log_event(manager, event);
    destroy_audit_event(event);
    return result;
}

// 轮转日志文件
bool audit_rotate_log(AuditManager* manager) {
    if (!manager) {
        return false;
    }
    
    return open_log_file(manager);
}

// 刷新日志
bool audit_flush_log(AuditManager* manager) {
    if (!manager || !manager->log_file) {
        return false;
    }
    
    return fflush(manager->log_file) == 0;
}

// 设置审计配置
bool audit_set_config(AuditManager* manager, AuditConfig* config) {
    if (!manager || !config) {
        return false;
    }
    
    // 释放旧配置
    if (manager->config) {
        if (manager->config->log_dir) {
            free(manager->config->log_dir);
        }
        if (manager->config->log_file) {
            free(manager->config->log_file);
        }
        if (manager->config->encryption_key) {
            free(manager->config->encryption_key);
        }
        free(manager->config);
    }
    
    // 更新配置
    manager->config = config;
    
    // 重新打开日志文件
    return open_log_file(manager);
}

// 获取审计配置
AuditConfig* audit_get_config(AuditManager* manager) {
    if (!manager) {
        return NULL;
    }
    
    return manager->config;
}

// 检查审计是否启用
bool audit_is_enabled(AuditManager* manager) {
    if (!manager || !manager->config) {
        return false;
    }
    
    return manager->config->enabled;
}

// 启用审计
bool audit_enable(AuditManager* manager) {
    if (!manager || !manager->config) {
        return false;
    }
    
    manager->config->enabled = true;
    return true;
}

// 禁用审计
bool audit_disable(AuditManager* manager) {
    if (!manager || !manager->config) {
        return false;
    }
    
    manager->config->enabled = false;
    return true;
}

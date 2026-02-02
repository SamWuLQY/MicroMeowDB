#include "monitoring.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 默认监控配置
static const monitoring_config default_monitoring_config = {
    .enabled = true,
    .max_stats = 1024,
    .collect_interval = 1000,
    .metrics_file = NULL,
    .export_prometheus = false,
    .prometheus_port = 9100
};

// 查找统计项
static stat *find_stat(monitoring_system *monitoring, const char *name) {
    if (!monitoring || !name) {
        return NULL;
    }
    
    stat *current = monitoring->stats;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

// 初始化监控系统
monitoring_system *monitoring_init(const monitoring_config *config) {
    monitoring_system *monitoring = (monitoring_system *)malloc(sizeof(monitoring_system));
    if (!monitoring) {
        return NULL;
    }
    
    // 使用默认配置或用户配置
    const monitoring_config *used_config = config ? config : &default_monitoring_config;
    
    monitoring->stats = NULL;
    monitoring->stat_count = 0;
    monitoring->max_stats = used_config->max_stats;
    monitoring->initialized = true;
    monitoring->enabled = used_config->enabled;
    monitoring->start_time = (uint64_t)time(NULL);
    monitoring->last_collect_time = 0;
    
    // 注册核心指标
    monitoring_register_core_metrics(monitoring);
    
    return monitoring;
}

// 销毁监控系统
void monitoring_destroy(monitoring_system *monitoring) {
    if (!monitoring) {
        return;
    }
    
    stat *current = monitoring->stats;
    while (current) {
        stat *next = current->next;
        if (current->name) {
            free(current->name);
        }
        if (current->description) {
            free(current->description);
        }
        free(current);
        current = next;
    }
    
    free(monitoring);
}

// 启用监控
void monitoring_enable(monitoring_system *monitoring) {
    if (monitoring) {
        monitoring->enabled = true;
    }
}

// 禁用监控
void monitoring_disable(monitoring_system *monitoring) {
    if (monitoring) {
        monitoring->enabled = false;
    }
}

// 创建统计项
static stat *create_stat(const char *name, stat_type type, const char *description) {
    stat *new_stat = (stat *)malloc(sizeof(stat));
    if (!new_stat) {
        return NULL;
    }
    
    new_stat->name = strdup(name);
    if (!new_stat->name) {
        free(new_stat);
        return NULL;
    }
    
    new_stat->type = type;
    new_stat->description = description ? strdup(description) : NULL;
    new_stat->next = NULL;
    
    // 初始化值
    switch (type) {
        case STAT_TYPE_COUNTER:
            new_stat->value.counter = 0;
            break;
        case STAT_TYPE_GAUGE:
            new_stat->value.gauge = 0.0;
            break;
        case STAT_TYPE_TIMER:
            new_stat->value.timer.count = 0;
            new_stat->value.timer.sum = 0.0;
            new_stat->value.timer.min = 0.0;
            new_stat->value.timer.max = 0.0;
            new_stat->value.timer.avg = 0.0;
            break;
    }
    
    return new_stat;
}

// 注册计数器
bool monitoring_register_counter(monitoring_system *monitoring, const char *name, const char *description) {
    if (!monitoring || !name || !monitoring->enabled) {
        return false;
    }
    
    if (find_stat(monitoring, name)) {
        return false;
    }
    
    if (monitoring->stat_count >= monitoring->max_stats) {
        return false;
    }
    
    stat *new_stat = create_stat(name, STAT_TYPE_COUNTER, description);
    if (!new_stat) {
        return false;
    }
    
    // 添加到列表
    if (!monitoring->stats) {
        monitoring->stats = new_stat;
    } else {
        stat *current = monitoring->stats;
        while (current->next) {
            current = current->next;
        }
        current->next = new_stat;
    }
    
    monitoring->stat_count++;
    return true;
}

// 注册仪表盘
bool monitoring_register_gauge(monitoring_system *monitoring, const char *name, const char *description) {
    if (!monitoring || !name || !monitoring->enabled) {
        return false;
    }
    
    if (find_stat(monitoring, name)) {
        return false;
    }
    
    if (monitoring->stat_count >= monitoring->max_stats) {
        return false;
    }
    
    stat *new_stat = create_stat(name, STAT_TYPE_GAUGE, description);
    if (!new_stat) {
        return false;
    }
    
    // 添加到列表
    if (!monitoring->stats) {
        monitoring->stats = new_stat;
    } else {
        stat *current = monitoring->stats;
        while (current->next) {
            current = current->next;
        }
        current->next = new_stat;
    }
    
    monitoring->stat_count++;
    return true;
}

// 注册计时器
bool monitoring_register_timer(monitoring_system *monitoring, const char *name, const char *description) {
    if (!monitoring || !name || !monitoring->enabled) {
        return false;
    }
    
    if (find_stat(monitoring, name)) {
        return false;
    }
    
    if (monitoring->stat_count >= monitoring->max_stats) {
        return false;
    }
    
    stat *new_stat = create_stat(name, STAT_TYPE_TIMER, description);
    if (!new_stat) {
        return false;
    }
    
    // 添加到列表
    if (!monitoring->stats) {
        monitoring->stats = new_stat;
    } else {
        stat *current = monitoring->stats;
        while (current->next) {
            current = current->next;
        }
        current->next = new_stat;
    }
    
    monitoring->stat_count++;
    return true;
}

// 增加计数器
void monitoring_increment_counter(monitoring_system *monitoring, const char *name, uint64_t value) {
    if (!monitoring || !name || !monitoring->enabled) {
        return;
    }
    
    stat *target = find_stat(monitoring, name);
    if (target && target->type == STAT_TYPE_COUNTER) {
        target->value.counter += value;
    }
}

// 设置仪表盘
void monitoring_set_gauge(monitoring_system *monitoring, const char *name, double value) {
    if (!monitoring || !name || !monitoring->enabled) {
        return;
    }
    
    stat *target = find_stat(monitoring, name);
    if (target && target->type == STAT_TYPE_GAUGE) {
        target->value.gauge = value;
    }
}

// 记录计时器
void monitoring_record_timer(monitoring_system *monitoring, const char *name, double value) {
    if (!monitoring || !name || !monitoring->enabled) {
        return;
    }
    
    stat *target = find_stat(monitoring, name);
    if (target && target->type == STAT_TYPE_TIMER) {
        target->value.timer.count++;
        target->value.timer.sum += value;
        target->value.timer.avg = target->value.timer.sum / target->value.timer.count;
        
        if (target->value.timer.count == 1 || value < target->value.timer.min) {
            target->value.timer.min = value;
        }
        if (value > target->value.timer.max) {
            target->value.timer.max = value;
        }
    }
}

// 获取统计值
bool monitoring_get_stat(monitoring_system *monitoring, const char *name, void *value) {
    if (!monitoring || !name || !value) {
        return false;
    }
    
    stat *target = find_stat(monitoring, name);
    if (!target) {
        return false;
    }
    
    switch (target->type) {
        case STAT_TYPE_COUNTER:
            *((uint64_t *)value) = target->value.counter;
            break;
        case STAT_TYPE_GAUGE:
            *((double *)value) = target->value.gauge;
            break;
        case STAT_TYPE_TIMER:
            memcpy(value, &target->value.timer, sizeof(target->value.timer));
            break;
        default:
            return false;
    }
    
    return true;
}

// 收集统计数据
void monitoring_collect(monitoring_system *monitoring) {
    if (!monitoring || !monitoring->enabled) {
        return;
    }
    
    // 记录收集时间
    monitoring->last_collect_time = (uint64_t)time(NULL);
    
    // 更新系统指标
    uint64_t uptime = monitoring->last_collect_time - monitoring->start_time;
    monitoring_set_gauge(monitoring, "system.uptime", (double)uptime);
    monitoring_set_gauge(monitoring, "system.stats_count", (double)monitoring->stat_count);
}

// 导出统计数据
bool monitoring_export(monitoring_system *monitoring, const char *format, char **output) {
    if (!monitoring || !format || !output) {
        return false;
    }
    
    // 收集数据
    monitoring_collect(monitoring);
    
    // 计算输出大小
    size_t output_size = 1024;
    *output = (char *)malloc(output_size);
    if (!*output) {
        return false;
    }
    
    // 构建输出
    char *pos = *output;
    size_t remaining = output_size;
    
    // 添加头部
    int written = snprintf(pos, remaining, "# MicroMeowDB Metrics\n# Timestamp: %llu\n\n", 
                          (unsigned long long)monitoring->last_collect_time);
    pos += written;
    remaining -= written;
    
    // 添加统计数据
    stat *current = monitoring->stats;
    while (current) {
        if (remaining < 1024) {
            output_size *= 2;
            *output = (char *)realloc(*output, output_size);
            if (!*output) {
                return false;
            }
            pos = *output + (output_size / 2);
            remaining = output_size / 2;
        }
        
        switch (current->type) {
            case STAT_TYPE_COUNTER:
                written = snprintf(pos, remaining, "%s %llu\n", 
                                  current->name, (unsigned long long)current->value.counter);
                break;
            case STAT_TYPE_GAUGE:
                written = snprintf(pos, remaining, "%s %f\n", 
                                  current->name, current->value.gauge);
                break;
            case STAT_TYPE_TIMER:
                written = snprintf(pos, remaining, "%s_count %llu\n%s_sum %f\n%s_min %f\n%s_max %f\n%s_avg %f\n", 
                                  current->name, (unsigned long long)current->value.timer.count,
                                  current->name, current->value.timer.sum,
                                  current->name, current->value.timer.min,
                                  current->name, current->value.timer.max,
                                  current->name, current->value.timer.avg);
                break;
        }
        
        pos += written;
        remaining -= written;
        current = current->next;
    }
    
    return true;
}

// 重置统计数据
void monitoring_reset(monitoring_system *monitoring) {
    if (!monitoring || !monitoring->enabled) {
        return;
    }
    
    stat *current = monitoring->stats;
    while (current) {
        switch (current->type) {
            case STAT_TYPE_COUNTER:
                current->value.counter = 0;
                break;
            case STAT_TYPE_GAUGE:
                current->value.gauge = 0.0;
                break;
            case STAT_TYPE_TIMER:
                current->value.timer.count = 0;
                current->value.timer.sum = 0.0;
                current->value.timer.min = 0.0;
                current->value.timer.max = 0.0;
                current->value.timer.avg = 0.0;
                break;
        }
        current = current->next;
    }
}

// 获取系统启动时间
uint64_t monitoring_get_uptime(monitoring_system *monitoring) {
    if (!monitoring) {
        return 0;
    }
    
    return (uint64_t)time(NULL) - monitoring->start_time;
}

// 注册核心指标
void monitoring_register_core_metrics(monitoring_system *monitoring) {
    if (!monitoring) {
        return;
    }
    
    // 系统指标
    monitoring_register_gauge(monitoring, "system.uptime", "System uptime in seconds");
    monitoring_register_gauge(monitoring, "system.stats_count", "Number of registered statistics");
    
    // 内存指标
    monitoring_register_gauge(monitoring, "memory.used", "Used memory in bytes");
    monitoring_register_gauge(monitoring, "memory.total", "Total memory in bytes");
    monitoring_register_gauge(monitoring, "memory.pool_usage", "Memory pool usage percentage");
    
    // 存储指标
    monitoring_register_gauge(monitoring, "storage.disk_used", "Used disk space in bytes");
    monitoring_register_gauge(monitoring, "storage.disk_total", "Total disk space in bytes");
    monitoring_register_counter(monitoring, "storage.io_reads", "Number of read operations");
    monitoring_register_counter(monitoring, "storage.io_writes", "Number of write operations");
    monitoring_register_timer(monitoring, "storage.io_read_time", "Read operation latency in milliseconds");
    monitoring_register_timer(monitoring, "storage.io_write_time", "Write operation latency in milliseconds");
    
    // 网络指标
    monitoring_register_gauge(monitoring, "network.connections", "Current number of connections");
    monitoring_register_gauge(monitoring, "network.max_connections", "Maximum number of connections");
    monitoring_register_counter(monitoring, "network.bytes_sent", "Bytes sent over network");
    monitoring_register_counter(monitoring, "network.bytes_received", "Bytes received over network");
    monitoring_register_counter(monitoring, "network.connection_errors", "Number of connection errors");
    
    // 事务指标
    monitoring_register_gauge(monitoring, "transaction.active", "Current active transactions");
    monitoring_register_gauge(monitoring, "transaction.max", "Maximum transactions");
    monitoring_register_counter(monitoring, "transaction.commits", "Number of committed transactions");
    monitoring_register_counter(monitoring, "transaction.rollbacks", "Number of rolled back transactions");
    monitoring_register_counter(monitoring, "transaction.deadlocks", "Number of deadlocks detected");
    monitoring_register_timer(monitoring, "transaction.duration", "Transaction duration in milliseconds");
    
    // 索引指标
    monitoring_register_counter(monitoring, "index.lookups", "Number of index lookups");
    monitoring_register_counter(monitoring, "index.hits", "Number of index hits");
    monitoring_register_counter(monitoring, "index.misses", "Number of index misses");
    monitoring_register_timer(monitoring, "index.lookup_time", "Index lookup time in milliseconds");
}

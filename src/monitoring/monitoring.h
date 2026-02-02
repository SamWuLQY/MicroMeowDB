#ifndef MONITORING_H
#define MONITORING_H

#include <stdint.h>
#include <stdbool.h>

// 统计类型
typedef enum {
    STAT_TYPE_COUNTER,
    STAT_TYPE_GAUGE,
    STAT_TYPE_TIMER
} stat_type;

// 统计指标结构
typedef struct {
    char *name;
    stat_type type;
    union {
        uint64_t counter;
        double gauge;
        struct {
            uint64_t count;
            double sum;
            double min;
            double max;
            double avg;
        } timer;
    } value;
    char *description;
    struct stat *next;
} stat;

// 监控系统结构
typedef struct {
    stat *stats;
    uint32_t stat_count;
    uint32_t max_stats;
    bool initialized;
    bool enabled;
    uint64_t start_time;
    uint64_t last_collect_time;
} monitoring_system;

// 监控配置结构
typedef struct {
    bool enabled;
    uint32_t max_stats;
    uint32_t collect_interval;
    char *metrics_file;
    bool export_prometheus;
    uint16_t prometheus_port;
} monitoring_config;

// 初始化监控系统
monitoring_system *monitoring_init(const monitoring_config *config);

// 销毁监控系统
void monitoring_destroy(monitoring_system *monitoring);

// 启用监控
void monitoring_enable(monitoring_system *monitoring);

// 禁用监控
void monitoring_disable(monitoring_system *monitoring);

// 注册计数器
bool monitoring_register_counter(monitoring_system *monitoring, const char *name, const char *description);

// 注册仪表盘
bool monitoring_register_gauge(monitoring_system *monitoring, const char *name, const char *description);

// 注册计时器
bool monitoring_register_timer(monitoring_system *monitoring, const char *name, const char *description);

// 增加计数器
void monitoring_increment_counter(monitoring_system *monitoring, const char *name, uint64_t value);

// 设置仪表盘
void monitoring_set_gauge(monitoring_system *monitoring, const char *name, double value);

// 记录计时器
void monitoring_record_timer(monitoring_system *monitoring, const char *name, double value);

// 获取统计值
bool monitoring_get_stat(monitoring_system *monitoring, const char *name, void *value);

// 收集统计数据
void monitoring_collect(monitoring_system *monitoring);

// 导出统计数据
bool monitoring_export(monitoring_system *monitoring, const char *format, char **output);

// 重置统计数据
void monitoring_reset(monitoring_system *monitoring);

// 获取系统启动时间
uint64_t monitoring_get_uptime(monitoring_system *monitoring);

// 数据库核心指标
// 注册所有核心指标
void monitoring_register_core_metrics(monitoring_system *monitoring);

#endif // MONITORING_H

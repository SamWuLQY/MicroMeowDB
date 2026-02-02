#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// 配置项类型
typedef enum {
    CONFIG_TYPE_INT,
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_DOUBLE
} config_type;

// 配置项结构
typedef struct {
    char *key;
    config_type type;
    union {
        int32_t int_val;
        bool bool_val;
        char *string_val;
        double double_val;
    } value;
    char *description;
    bool is_default;
} config_item;

// 配置系统结构
typedef struct {
    config_item **items;
    uint32_t count;
    uint32_t capacity;
    char *config_file;
    bool loaded;
} config_system;

// 配置模块枚举
typedef enum {
    CONFIG_MODULE_GENERAL,
    CONFIG_MODULE_SECURITY,
    CONFIG_MODULE_STORAGE,
    CONFIG_MODULE_MEMORY,
    CONFIG_MODULE_INDEX,
    CONFIG_MODULE_NETWORK,
    CONFIG_MODULE_LOGGING
} config_module;

// 初始化配置系统
config_system *config_init(const char *config_file);

// 销毁配置系统
void config_destroy(config_system *config);

// 加载配置文件
bool config_load(config_system *config);

// 保存配置文件
bool config_save(config_system *config);

// 获取整数配置
int32_t config_get_int(config_system *config, const char *key, int32_t default_val);

// 获取布尔配置
bool config_get_bool(config_system *config, const char *key, bool default_val);

// 获取字符串配置
const char *config_get_string(config_system *config, const char *key, const char *default_val);

// 获取浮点数配置
double config_get_double(config_system *config, const char *key, double default_val);

// 设置整数配置
bool config_set_int(config_system *config, const char *key, int32_t value, const char *description);

// 设置布尔配置
bool config_set_bool(config_system *config, const char *key, bool value, const char *description);

// 设置字符串配置
bool config_set_string(config_system *config, const char *key, const char *value, const char *description);

// 设置浮点数配置
bool config_set_double(config_system *config, const char *key, double value, const char *description);

// 注册默认配置
void config_register_defaults(config_system *config);

// 打印配置
void config_print(config_system *config, FILE *stream);

#endif // CONFIG_H

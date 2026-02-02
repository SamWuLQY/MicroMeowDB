#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// 用户状态定义
#define USER_STATUS_ACTIVE 0
#define USER_STATUS_LOCKED 1
#define USER_STATUS_DISABLED 2

// 密码哈希算法类型
#define PASSWORD_HASH_SHA256 0
#define PASSWORD_HASH_SHA512 1
#define PASSWORD_HASH_BCRYPT 2

// 用户结构
typedef struct {
    char* username;
    char* password_hash;
    char* salt;
    int password_hash_algorithm;
    int status;
    time_t created_at;
    time_t last_login;
    time_t password_changed_at;
    struct User* next;
    struct User* prev;
} User;

// 认证管理结构
typedef struct {
    User* users;
    size_t user_count;
    char* encryption_key; // 用于加密敏感信息的密钥
    int max_login_attempts; // 最大登录尝试次数
    int lockout_duration; // 锁定持续时间（秒）
} AuthenticationManager;

// 初始化认证管理器
AuthenticationManager* auth_manager_init(const char* encryption_key, int max_login_attempts, int lockout_duration);

// 创建用户
bool auth_create_user(AuthenticationManager* manager, const char* username, const char* password);

// 验证用户密码
bool auth_verify_password(AuthenticationManager* manager, const char* username, const char* password);

// 锁定用户
bool auth_lock_user(AuthenticationManager* manager, const char* username);

// 解锁用户
bool auth_unlock_user(AuthenticationManager* manager, const char* username);

// 禁用用户
bool auth_disable_user(AuthenticationManager* manager, const char* username);

// 启用用户
bool auth_enable_user(AuthenticationManager* manager, const char* username);

// 修改用户密码
bool auth_change_password(AuthenticationManager* manager, const char* username, const char* old_password, const char* new_password);

// 删除用户
bool auth_delete_user(AuthenticationManager* manager, const char* username);

// 获取用户信息
User* auth_get_user(AuthenticationManager* manager, const char* username);

// 检查用户状态
bool auth_check_user_status(AuthenticationManager* manager, const char* username);

// 记录登录尝试
bool auth_record_login_attempt(AuthenticationManager* manager, const char* username, bool success);

// 生成密码哈希
char* auth_generate_password_hash(const char* password, const char* salt, int algorithm);

// 生成随机盐
char* auth_generate_salt(size_t length);

// 销毁认证管理器
void auth_manager_destroy(AuthenticationManager* manager);

#endif // AUTHENTICATION_H
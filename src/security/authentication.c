#include "authentication.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

// 登录尝试记录结构
typedef struct {
    char* username;
    int failed_attempts;
    time_t last_failed_attempt;
    struct LoginAttempt* next;
} LoginAttempt;

// 认证管理器扩展结构
typedef struct {
    AuthenticationManager base;
    LoginAttempt* login_attempts;
} AuthenticationManagerEx;

// 生成随机盐
char* auth_generate_salt(size_t length) {
    if (length == 0) {
        length = 16;
    }

    char* salt = (char*)malloc(length + 1);
    if (!salt) {
        return NULL;
    }

    const char* charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t charset_len = strlen(charset);

    for (size_t i = 0; i < length; i++) {
        int rand_idx = rand() % charset_len;
        salt[i] = charset[rand_idx];
    }

    salt[length] = '\0';
    return salt;
}

// 生成密码哈希
char* auth_generate_password_hash(const char* password, const char* salt, int algorithm) {
    if (!password || !salt) {
        return NULL;
    }

    char* combined = (char*)malloc(strlen(password) + strlen(salt) + 1);
    if (!combined) {
        return NULL;
    }

    strcpy(combined, password);
    strcat(combined, salt);

    char* hash = NULL;

    switch (algorithm) {
        case PASSWORD_HASH_SHA256: {
            unsigned char sha256_hash[SHA256_DIGEST_LENGTH];
            SHA256((unsigned char*)combined, strlen(combined), sha256_hash);

            hash = (char*)malloc(SHA256_DIGEST_LENGTH * 2 + 1);
            if (hash) {
                for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
                    sprintf(&hash[i * 2], "%02x", sha256_hash[i]);
                }
                hash[SHA256_DIGEST_LENGTH * 2] = '\0';
            }
            break;
        }
        case PASSWORD_HASH_SHA512: {
            unsigned char sha512_hash[SHA512_DIGEST_LENGTH];
            SHA512((unsigned char*)combined, strlen(combined), sha512_hash);

            hash = (char*)malloc(SHA512_DIGEST_LENGTH * 2 + 1);
            if (hash) {
                for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
                    sprintf(&hash[i * 2], "%02x", sha512_hash[i]);
                }
                hash[SHA512_DIGEST_LENGTH * 2] = '\0';
            }
            break;
        }
        case PASSWORD_HASH_BCRYPT:
            // BCRYPT 实现需要额外的库支持
            // 这里暂时使用 SHA512 作为替代
            fprintf(stderr, "BCRYPT not implemented, using SHA512 instead\n");
            unsigned char sha512_hash[SHA512_DIGEST_LENGTH];
            SHA512((unsigned char*)combined, strlen(combined), sha512_hash);

            hash = (char*)malloc(SHA512_DIGEST_LENGTH * 2 + 1);
            if (hash) {
                for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
                    sprintf(&hash[i * 2], "%02x", sha512_hash[i]);
                }
                hash[SHA512_DIGEST_LENGTH * 2] = '\0';
            }
            break;
        default:
            fprintf(stderr, "Unknown hash algorithm\n");
            break;
    }

    free(combined);
    return hash;
}

// 初始化认证管理器
AuthenticationManager* auth_manager_init(const char* encryption_key, int max_login_attempts, int lockout_duration) {
    AuthenticationManagerEx* manager = (AuthenticationManagerEx*)malloc(sizeof(AuthenticationManagerEx));
    if (!manager) {
        return NULL;
    }

    manager->base.users = NULL;
    manager->base.user_count = 0;
    manager->base.encryption_key = encryption_key ? strdup(encryption_key) : NULL;
    manager->base.max_login_attempts = max_login_attempts > 0 ? max_login_attempts : 5;
    manager->base.lockout_duration = lockout_duration > 0 ? lockout_duration : 300; // 默认5分钟
    manager->login_attempts = NULL;

    return &manager->base;
}

// 获取用户信息
User* auth_get_user(AuthenticationManager* manager, const char* username) {
    if (!manager || !username) {
        return NULL;
    }

    User* current = manager->users;
    while (current) {
        if (strcmp(current->username, username) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

// 创建用户
bool auth_create_user(AuthenticationManager* manager, const char* username, const char* password) {
    if (!manager || !username || !password) {
        return false;
    }

    // 检查用户是否已存在
    if (auth_get_user(manager, username)) {
        fprintf(stderr, "User already exists\n");
        return false;
    }

    // 生成盐
    char* salt = auth_generate_salt(16);
    if (!salt) {
        return false;
    }

    // 生成密码哈希
    char* password_hash = auth_generate_password_hash(password, salt, PASSWORD_HASH_SHA512);
    if (!password_hash) {
        free(salt);
        return false;
    }

    // 创建用户结构
    User* user = (User*)malloc(sizeof(User));
    if (!user) {
        free(salt);
        free(password_hash);
        return false;
    }

    user->username = strdup(username);
    user->password_hash = password_hash;
    user->salt = salt;
    user->password_hash_algorithm = PASSWORD_HASH_SHA512;
    user->status = USER_STATUS_ACTIVE;
    user->created_at = time(NULL);
    user->last_login = 0;
    user->password_changed_at = time(NULL);
    user->next = NULL;
    user->prev = NULL;

    // 添加到用户列表
    if (!manager->users) {
        manager->users = user;
    } else {
        User* current = manager->users;
        while (current->next) {
            current = current->next;
        }
        current->next = user;
        user->prev = current;
    }

    manager->user_count++;
    return true;
}

// 验证用户密码
bool auth_verify_password(AuthenticationManager* manager, const char* username, const char* password) {
    if (!manager || !username || !password) {
        return false;
    }

    // 获取用户
    User* user = auth_get_user(manager, username);
    if (!user) {
        return false;
    }

    // 检查用户状态
    if (user->status != USER_STATUS_ACTIVE) {
        return false;
    }

    // 生成密码哈希并验证
    char* generated_hash = auth_generate_password_hash(password, user->salt, user->password_hash_algorithm);
    if (!generated_hash) {
        return false;
    }

    bool result = (strcmp(generated_hash, user->password_hash) == 0);
    free(generated_hash);

    // 记录登录尝试
    auth_record_login_attempt(manager, username, result);

    // 更新最后登录时间
    if (result) {
        user->last_login = time(NULL);
    }

    return result;
}

// 锁定用户
bool auth_lock_user(AuthenticationManager* manager, const char* username) {
    User* user = auth_get_user(manager, username);
    if (!user) {
        return false;
    }

    user->status = USER_STATUS_LOCKED;
    return true;
}

// 解锁用户
bool auth_unlock_user(AuthenticationManager* manager, const char* username) {
    User* user = auth_get_user(manager, username);
    if (!user) {
        return false;
    }

    user->status = USER_STATUS_ACTIVE;
    return true;
}

// 禁用用户
bool auth_disable_user(AuthenticationManager* manager, const char* username) {
    User* user = auth_get_user(manager, username);
    if (!user) {
        return false;
    }

    user->status = USER_STATUS_DISABLED;
    return true;
}

// 启用用户
bool auth_enable_user(AuthenticationManager* manager, const char* username) {
    User* user = auth_get_user(manager, username);
    if (!user) {
        return false;
    }

    user->status = USER_STATUS_ACTIVE;
    return true;
}

// 修改用户密码
bool auth_change_password(AuthenticationManager* manager, const char* username, const char* old_password, const char* new_password) {
    if (!manager || !username || !old_password || !new_password) {
        return false;
    }

    // 验证旧密码
    if (!auth_verify_password(manager, username, old_password)) {
        return false;
    }

    // 获取用户
    User* user = auth_get_user(manager, username);
    if (!user) {
        return false;
    }

    // 生成新盐
    char* new_salt = auth_generate_salt(16);
    if (!new_salt) {
        return false;
    }

    // 生成新密码哈希
    char* new_password_hash = auth_generate_password_hash(new_password, new_salt, user->password_hash_algorithm);
    if (!new_password_hash) {
        free(new_salt);
        return false;
    }

    // 更新密码信息
    free(user->password_hash);
    free(user->salt);
    user->password_hash = new_password_hash;
    user->salt = new_salt;
    user->password_changed_at = time(NULL);

    return true;
}

// 删除用户
bool auth_delete_user(AuthenticationManager* manager, const char* username) {
    if (!manager || !username) {
        return false;
    }

    // 获取用户
    User* user = auth_get_user(manager, username);
    if (!user) {
        return false;
    }

    // 从链表中移除
    if (user->prev) {
        user->prev->next = user->next;
    } else {
        manager->users = user->next;
    }

    if (user->next) {
        user->next->prev = user->prev;
    }

    // 释放资源
    free(user->username);
    free(user->password_hash);
    free(user->salt);
    free(user);

    manager->user_count--;
    return true;
}

// 检查用户状态
bool auth_check_user_status(AuthenticationManager* manager, const char* username) {
    if (!manager || !username) {
        return false;
    }

    User* user = auth_get_user(manager, username);
    if (!user) {
        return false;
    }

    return (user->status == USER_STATUS_ACTIVE);
}

// 记录登录尝试
bool auth_record_login_attempt(AuthenticationManager* manager, const char* username, bool success) {
    if (!manager || !username) {
        return false;
    }

    AuthenticationManagerEx* ex_manager = (AuthenticationManagerEx*)manager;

    // 查找或创建登录尝试记录
    LoginAttempt* attempt = ex_manager->login_attempts;
    while (attempt) {
        if (strcmp(attempt->username, username) == 0) {
            break;
        }
        attempt = attempt->next;
    }

    if (!attempt) {
        // 创建新记录
        attempt = (LoginAttempt*)malloc(sizeof(LoginAttempt));
        if (!attempt) {
            return false;
        }

        attempt->username = strdup(username);
        attempt->failed_attempts = 0;
        attempt->last_failed_attempt = 0;
        attempt->next = ex_manager->login_attempts;
        ex_manager->login_attempts = attempt;
    }

    if (success) {
        // 登录成功，重置失败计数
        attempt->failed_attempts = 0;
    } else {
        // 登录失败，增加失败计数
        attempt->failed_attempts++;
        attempt->last_failed_attempt = time(NULL);

        // 检查是否需要锁定用户
        if (attempt->failed_attempts >= manager->max_login_attempts) {
            auth_lock_user(manager, username);
        }
    }

    return true;
}

// 销毁认证管理器
void auth_manager_destroy(AuthenticationManager* manager) {
    if (!manager) {
        return;
    }

    // 释放用户列表
    User* current_user = manager->users;
    while (current_user) {
        User* next_user = current_user->next;
        free(current_user->username);
        free(current_user->password_hash);
        free(current_user->salt);
        free(current_user);
        current_user = next_user;
    }

    // 释放登录尝试记录
    AuthenticationManagerEx* ex_manager = (AuthenticationManagerEx*)manager;
    LoginAttempt* current_attempt = ex_manager->login_attempts;
    while (current_attempt) {
        LoginAttempt* next_attempt = current_attempt->next;
        free(current_attempt->username);
        free(current_attempt);
        current_attempt = next_attempt;
    }

    // 释放加密密钥
    if (manager->encryption_key) {
        free((void*)manager->encryption_key);
    }

    free(manager);
}
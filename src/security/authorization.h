#ifndef AUTHORIZATION_H
#define AUTHORIZATION_H

#include <stdint.h>
#include <stdbool.h>
#include "authentication.h"

// 权限类型定义
#define PRIVILEGE_SELECT 0x0001
#define PRIVILEGE_INSERT 0x0002
#define PRIVILEGE_UPDATE 0x0004
#define PRIVILEGE_DELETE 0x0008
#define PRIVILEGE_CREATE 0x0010
#define PRIVILEGE_DROP 0x0020
#define PRIVILEGE_ALTER 0x0040
#define PRIVILEGE_EXECUTE 0x0080
#define PRIVILEGE_ALL 0xFFFF

// 权限作用域类型
#define SCOPE_GLOBAL 0
#define SCOPE_DATABASE 1
#define SCOPE_TABLE 2
#define SCOPE_COLUMN 3

// 角色结构
typedef struct {
    char* role_name;
    uint16_t privileges;
    struct Role* next;
    struct Role* prev;
} Role;

// 权限结构
typedef struct {
    char* username;
    int scope_type;
    char* scope_name; // 数据库名、表名或列名
    uint16_t privileges;
    struct Privilege* next;
    struct Privilege* prev;
} Privilege;

// 授权管理结构
typedef struct {
    Role* roles;
    size_t role_count;
    Privilege* privileges;
    size_t privilege_count;
    AuthenticationManager* auth_manager;
} AuthorizationManager;

// 初始化授权管理器
AuthorizationManager* authz_manager_init(AuthenticationManager* auth_manager);

// 创建角色
bool authz_create_role(AuthorizationManager* manager, const char* role_name, uint16_t privileges);

// 删除角色
bool authz_drop_role(AuthorizationManager* manager, const char* role_name);

// 给角色授予权限
bool authz_grant_role_privileges(AuthorizationManager* manager, const char* role_name, uint16_t privileges);

// 从角色撤销权限
bool authz_revoke_role_privileges(AuthorizationManager* manager, const char* role_name, uint16_t privileges);

// 给用户授予角色
bool authz_grant_role_to_user(AuthorizationManager* manager, const char* username, const char* role_name);

// 从用户撤销角色
bool authz_revoke_role_from_user(AuthorizationManager* manager, const char* username, const char* role_name);

// 给用户授予权限
bool authz_grant_privilege(AuthorizationManager* manager, const char* username, int scope_type, const char* scope_name, uint16_t privileges);

// 从用户撤销权限
bool authz_revoke_privilege(AuthorizationManager* manager, const char* username, int scope_type, const char* scope_name, uint16_t privileges);

// 检查用户是否有指定权限
bool authz_check_privilege(AuthorizationManager* manager, const char* username, int scope_type, const char* scope_name, uint16_t privilege);

// 获取用户的所有权限
uint16_t authz_get_user_privileges(AuthorizationManager* manager, const char* username, int scope_type, const char* scope_name);

// 获取角色信息
Role* authz_get_role(AuthorizationManager* manager, const char* role_name);

// 获取用户的角色列表
Role** authz_get_user_roles(AuthorizationManager* manager, const char* username, size_t* role_count);

// 销毁授权管理器
void authz_manager_destroy(AuthorizationManager* manager);

#endif // AUTHORIZATION_H
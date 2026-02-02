#include "authorization.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 用户-角色映射结构
typedef struct {
    char* username;
    char* role_name;
    struct UserRole* next;
} UserRole;

// 授权管理器扩展结构
typedef struct {
    AuthorizationManager base;
    UserRole* user_roles;
    size_t user_role_count;
} AuthorizationManagerEx;

// 初始化授权管理器
AuthorizationManager* authz_manager_init(AuthenticationManager* auth_manager) {
    AuthorizationManagerEx* manager = (AuthorizationManagerEx*)malloc(sizeof(AuthorizationManagerEx));
    if (!manager) {
        return NULL;
    }

    manager->base.roles = NULL;
    manager->base.role_count = 0;
    manager->base.privileges = NULL;
    manager->base.privilege_count = 0;
    manager->base.auth_manager = auth_manager;
    manager->user_roles = NULL;
    manager->user_role_count = 0;

    return &manager->base;
}

// 获取角色信息
Role* authz_get_role(AuthorizationManager* manager, const char* role_name) {
    if (!manager || !role_name) {
        return NULL;
    }

    Role* current = manager->roles;
    while (current) {
        if (strcmp(current->role_name, role_name) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

// 创建角色
bool authz_create_role(AuthorizationManager* manager, const char* role_name, uint16_t privileges) {
    if (!manager || !role_name) {
        return false;
    }

    // 检查角色是否已存在
    if (authz_get_role(manager, role_name)) {
        fprintf(stderr, "Role already exists\n");
        return false;
    }

    // 创建角色结构
    Role* role = (Role*)malloc(sizeof(Role));
    if (!role) {
        return false;
    }

    role->role_name = strdup(role_name);
    role->privileges = privileges;
    role->next = NULL;
    role->prev = NULL;

    // 添加到角色列表
    if (!manager->roles) {
        manager->roles = role;
    } else {
        Role* current = manager->roles;
        while (current->next) {
            current = current->next;
        }
        current->next = role;
        role->prev = current;
    }

    manager->role_count++;
    return true;
}

// 删除角色
bool authz_drop_role(AuthorizationManager* manager, const char* role_name) {
    if (!manager || !role_name) {
        return false;
    }

    // 查找角色
    Role* role = authz_get_role(manager, role_name);
    if (!role) {
        return false;
    }

    // 从链表中移除
    if (role->prev) {
        role->prev->next = role->next;
    } else {
        manager->roles = role->next;
    }

    if (role->next) {
        role->next->prev = role->prev;
    }

    // 释放资源
    free(role->role_name);
    free(role);

    manager->role_count--;
    return true;
}

// 给角色授予权限
bool authz_grant_role_privileges(AuthorizationManager* manager, const char* role_name, uint16_t privileges) {
    if (!manager || !role_name) {
        return false;
    }

    // 查找角色
    Role* role = authz_get_role(manager, role_name);
    if (!role) {
        return false;
    }

    // 授予权限
    role->privileges |= privileges;
    return true;
}

// 从角色撤销权限
bool authz_revoke_role_privileges(AuthorizationManager* manager, const char* role_name, uint16_t privileges) {
    if (!manager || !role_name) {
        return false;
    }

    // 查找角色
    Role* role = authz_get_role(manager, role_name);
    if (!role) {
        return false;
    }

    // 撤销权限
    role->privileges &= ~privileges;
    return true;
}

// 给用户授予角色
bool authz_grant_role_to_user(AuthorizationManager* manager, const char* username, const char* role_name) {
    if (!manager || !username || !role_name) {
        return false;
    }

    // 检查用户是否存在
    if (!auth_get_user(manager->auth_manager, username)) {
        fprintf(stderr, "User does not exist\n");
        return false;
    }

    // 检查角色是否存在
    if (!authz_get_role(manager, role_name)) {
        fprintf(stderr, "Role does not exist\n");
        return false;
    }

    AuthorizationManagerEx* ex_manager = (AuthorizationManagerEx*)manager;

    // 检查用户-角色映射是否已存在
    UserRole* current = ex_manager->user_roles;
    while (current) {
        if (strcmp(current->username, username) == 0 && strcmp(current->role_name, role_name) == 0) {
            fprintf(stderr, "User already has this role\n");
            return false;
        }
        current = current->next;
    }

    // 创建用户-角色映射
    UserRole* user_role = (UserRole*)malloc(sizeof(UserRole));
    if (!user_role) {
        return false;
    }

    user_role->username = strdup(username);
    user_role->role_name = strdup(role_name);
    user_role->next = ex_manager->user_roles;

    ex_manager->user_roles = user_role;
    ex_manager->user_role_count++;

    return true;
}

// 从用户撤销角色
bool authz_revoke_role_from_user(AuthorizationManager* manager, const char* username, const char* role_name) {
    if (!manager || !username || !role_name) {
        return false;
    }

    AuthorizationManagerEx* ex_manager = (AuthorizationManagerEx*)manager;

    // 查找用户-角色映射
    UserRole* current = ex_manager->user_roles;
    UserRole* prev = NULL;

    while (current) {
        if (strcmp(current->username, username) == 0 && strcmp(current->role_name, role_name) == 0) {
            break;
        }
        prev = current;
        current = current->next;
    }

    if (!current) {
        return false;
    }

    // 从链表中移除
    if (prev) {
        prev->next = current->next;
    } else {
        ex_manager->user_roles = current->next;
    }

    // 释放资源
    free(current->username);
    free(current->role_name);
    free(current);

    ex_manager->user_role_count--;
    return true;
}

// 给用户授予权限
bool authz_grant_privilege(AuthorizationManager* manager, const char* username, int scope_type, const char* scope_name, uint16_t privileges) {
    if (!manager || !username) {
        return false;
    }

    // 检查用户是否存在
    if (!auth_get_user(manager->auth_manager, username)) {
        fprintf(stderr, "User does not exist\n");
        return false;
    }

    // 检查权限作用域类型
    if (scope_type < SCOPE_GLOBAL || scope_type > SCOPE_COLUMN) {
        fprintf(stderr, "Invalid scope type\n");
        return false;
    }

    // 查找或创建权限记录
    Privilege* privilege = manager->privileges;
    while (privilege) {
        if (strcmp(privilege->username, username) == 0 && 
            privilege->scope_type == scope_type && 
            (scope_name == NULL || strcmp(privilege->scope_name, scope_name) == 0)) {
            break;
        }
        privilege = privilege->next;
    }

    if (privilege) {
        // 更新现有权限
        privilege->privileges |= privileges;
    } else {
        // 创建新权限记录
        privilege = (Privilege*)malloc(sizeof(Privilege));
        if (!privilege) {
            return false;
        }

        privilege->username = strdup(username);
        privilege->scope_type = scope_type;
        privilege->scope_name = scope_name ? strdup(scope_name) : NULL;
        privilege->privileges = privileges;
        privilege->next = manager->privileges;
        privilege->prev = NULL;

        if (manager->privileges) {
            manager->privileges->prev = privilege;
        }
        manager->privileges = privilege;
        manager->privilege_count++;
    }

    return true;
}

// 从用户撤销权限
bool authz_revoke_privilege(AuthorizationManager* manager, const char* username, int scope_type, const char* scope_name, uint16_t privileges) {
    if (!manager || !username) {
        return false;
    }

    // 查找权限记录
    Privilege* privilege = manager->privileges;
    while (privilege) {
        if (strcmp(privilege->username, username) == 0 && 
            privilege->scope_type == scope_type && 
            (scope_name == NULL || strcmp(privilege->scope_name, scope_name) == 0)) {
            break;
        }
        privilege = privilege->next;
    }

    if (!privilege) {
        return false;
    }

    // 撤销权限
    privilege->privileges &= ~privileges;

    // 如果权限为0，删除权限记录
    if (privilege->privileges == 0) {
        if (privilege->prev) {
            privilege->prev->next = privilege->next;
        } else {
            manager->privileges = privilege->next;
        }

        if (privilege->next) {
            privilege->next->prev = privilege->prev;
        }

        free(privilege->username);
        if (privilege->scope_name) {
            free(privilege->scope_name);
        }
        free(privilege);

        manager->privilege_count--;
    }

    return true;
}

// 获取用户的所有权限
uint16_t authz_get_user_privileges(AuthorizationManager* manager, const char* username, int scope_type, const char* scope_name) {
    if (!manager || !username) {
        return 0;
    }

    uint16_t total_privileges = 0;

    // 检查用户直接权限
    Privilege* privilege = manager->privileges;
    while (privilege) {
        if (strcmp(privilege->username, username) == 0) {
            // 全局权限
            if (privilege->scope_type == SCOPE_GLOBAL) {
                total_privileges |= privilege->privileges;
            }
            // 数据库级权限
            else if (privilege->scope_type == SCOPE_DATABASE && 
                     scope_type >= SCOPE_DATABASE && 
                     (scope_name == NULL || strstr(scope_name, privilege->scope_name) == scope_name)) {
                total_privileges |= privilege->privileges;
            }
            // 表级权限
            else if (privilege->scope_type == SCOPE_TABLE && 
                     scope_type >= SCOPE_TABLE && 
                     scope_name != NULL && 
                     strcmp(privilege->scope_name, scope_name) == 0) {
                total_privileges |= privilege->privileges;
            }
            // 列级权限
            else if (privilege->scope_type == SCOPE_COLUMN && 
                     scope_type == SCOPE_COLUMN && 
                     scope_name != NULL && 
                     strcmp(privilege->scope_name, scope_name) == 0) {
                total_privileges |= privilege->privileges;
            }
        }
        privilege = privilege->next;
    }

    // 检查用户通过角色获得的权限
    AuthorizationManagerEx* ex_manager = (AuthorizationManagerEx*)manager;
    UserRole* user_role = ex_manager->user_roles;
    while (user_role) {
        if (strcmp(user_role->username, username) == 0) {
            Role* role = authz_get_role(manager, user_role->role_name);
            if (role) {
                total_privileges |= role->privileges;
            }
        }
        user_role = user_role->next;
    }

    return total_privileges;
}

// 检查用户是否有指定权限
bool authz_check_privilege(AuthorizationManager* manager, const char* username, int scope_type, const char* scope_name, uint16_t privilege) {
    if (!manager || !username) {
        return false;
    }

    // 检查用户是否存在且活跃
    if (!auth_check_user_status(manager->auth_manager, username)) {
        return false;
    }

    // 获取用户权限
    uint16_t user_privileges = authz_get_user_privileges(manager, username, scope_type, scope_name);

    // 检查是否有指定权限
    return (user_privileges & privilege) != 0;
}

// 获取用户的角色列表
Role** authz_get_user_roles(AuthorizationManager* manager, const char* username, size_t* role_count) {
    if (!manager || !username || !role_count) {
        return NULL;
    }

    *role_count = 0;

    AuthorizationManagerEx* ex_manager = (AuthorizationManagerEx*)manager;

    // 计算用户角色数量
    UserRole* user_role = ex_manager->user_roles;
    while (user_role) {
        if (strcmp(user_role->username, username) == 0) {
            (*role_count)++;
        }
        user_role = user_role->next;
    }

    if (*role_count == 0) {
        return NULL;
    }

    // 分配角色列表
    Role** roles = (Role**)malloc(sizeof(Role*) * (*role_count));
    if (!roles) {
        return NULL;
    }

    // 填充角色列表
    size_t index = 0;
    user_role = ex_manager->user_roles;
    while (user_role) {
        if (strcmp(user_role->username, username) == 0) {
            Role* role = authz_get_role(manager, user_role->role_name);
            if (role) {
                roles[index++] = role;
            }
        }
        user_role = user_role->next;
    }

    return roles;
}

// 销毁授权管理器
void authz_manager_destroy(AuthorizationManager* manager) {
    if (!manager) {
        return;
    }

    // 释放角色列表
    Role* current_role = manager->roles;
    while (current_role) {
        Role* next_role = current_role->next;
        free(current_role->role_name);
        free(current_role);
        current_role = next_role;
    }

    // 释放权限列表
    Privilege* current_privilege = manager->privileges;
    while (current_privilege) {
        Privilege* next_privilege = current_privilege->next;
        free(current_privilege->username);
        if (current_privilege->scope_name) {
            free(current_privilege->scope_name);
        }
        free(current_privilege);
        current_privilege = next_privilege;
    }

    // 释放用户-角色映射
    AuthorizationManagerEx* ex_manager = (AuthorizationManagerEx*)manager;
    UserRole* current_user_role = ex_manager->user_roles;
    while (current_user_role) {
        UserRole* next_user_role = current_user_role->next;
        free(current_user_role->username);
        free(current_user_role->role_name);
        free(current_user_role);
        current_user_role = next_user_role;
    }

    free(manager);
}
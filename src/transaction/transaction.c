#include "transaction.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 默认事务配置
static const transaction_config default_transaction_config = {
    .default_isolation = ISOLATION_LEVEL_REPEATABLE_READ,
    .max_transactions = 1000,
    .transaction_timeout = 300000,
    .enable_undo_log = true,
    .enable_redo_log = true
};

// 初始化事务管理器
transaction_manager *transaction_manager_init(const transaction_config *config) {
    transaction_manager *manager = (transaction_manager *)malloc(sizeof(transaction_manager));
    if (!manager) {
        return NULL;
    }
    
    // 使用默认配置或用户配置
    const transaction_config *used_config = config ? config : &default_transaction_config;
    
    manager->transactions = NULL;
    manager->locks = NULL;
    manager->next_transaction_id = 1;
    manager->active_transactions = 0;
    manager->max_transactions = used_config->max_transactions;
    manager->initialized = true;
    
    return manager;
}

// 销毁事务管理器
void transaction_manager_destroy(transaction_manager *manager) {
    if (!manager) {
        return;
    }
    
    // 清理事务
    transaction *current_tx = manager->transactions;
    while (current_tx) {
        transaction *next_tx = current_tx->next;
        
        // 清理锁
        lock *current_lock = current_tx->locks;
        while (current_lock) {
            lock *next_lock = current_lock->next;
            free(current_lock);
            current_lock = next_lock;
        }
        
        // 清理日志
        if (current_tx->undo_log) {
            free(current_tx->undo_log);
        }
        if (current_tx->redo_log) {
            free(current_tx->redo_log);
        }
        
        free(current_tx);
        current_tx = next_tx;
    }
    
    // 清理全局锁
    lock *current_lock = manager->locks;
    while (current_lock) {
        lock *next_lock = current_lock->next;
        free(current_lock);
        current_lock = next_lock;
    }
    
    free(manager);
}

// 开始事务
transaction *transaction_begin(transaction_manager *manager, isolation_level isolation) {
    if (!manager || !manager->initialized || manager->active_transactions >= manager->max_transactions) {
        return NULL;
    }
    
    // 创建事务
    transaction *tx = (transaction *)malloc(sizeof(transaction));
    if (!tx) {
        return NULL;
    }
    
    // 设置事务属性
    tx->id = manager->next_transaction_id++;
    tx->state = TRANSACTION_STATE_ACTIVE;
    tx->isolation = isolation != 0 ? isolation : default_transaction_config.default_isolation;
    tx->start_time = (uint64_t)time(NULL);
    tx->commit_time = 0;
    tx->locks = NULL;
    tx->undo_log = NULL;
    tx->redo_log = NULL;
    tx->next = NULL;
    
    // 添加到事务列表
    if (!manager->transactions) {
        manager->transactions = tx;
    } else {
        transaction *current = manager->transactions;
        while (current->next) {
            current = current->next;
        }
        current->next = tx;
    }
    
    manager->active_transactions++;
    return tx;
}

// 提交事务
bool transaction_commit(transaction_manager *manager, transaction *tx) {
    if (!manager || !tx || tx->state != TRANSACTION_STATE_ACTIVE) {
        return false;
    }
    
    // 释放锁
    lock *current_lock = tx->locks;
    while (current_lock) {
        lock *next_lock = current_lock->next;
        
        // 从全局锁列表中移除
        lock *prev_global_lock = NULL;
        lock *global_lock = manager->locks;
        while (global_lock) {
            if (global_lock->transaction_id == tx->id && global_lock->resource_id == current_lock->resource_id) {
                if (prev_global_lock) {
                    prev_global_lock->next = global_lock->next;
                } else {
                    manager->locks = global_lock->next;
                }
                free(global_lock);
                break;
            }
            prev_global_lock = global_lock;
            global_lock = global_lock->next;
        }
        
        free(current_lock);
        current_lock = next_lock;
    }
    tx->locks = NULL;
    
    // 清理日志
    if (tx->undo_log) {
        free(tx->undo_log);
        tx->undo_log = NULL;
    }
    if (tx->redo_log) {
        free(tx->redo_log);
        tx->redo_log = NULL;
    }
    
    // 更新事务状态
    tx->state = TRANSACTION_STATE_COMMITTED;
    tx->commit_time = (uint64_t)time(NULL);
    manager->active_transactions--;
    
    return true;
}

// 回滚事务
bool transaction_rollback(transaction_manager *manager, transaction *tx) {
    if (!manager || !tx || (tx->state != TRANSACTION_STATE_ACTIVE && tx->state != TRANSACTION_STATE_PREPARED)) {
        return false;
    }
    
    // 释放锁
    lock *current_lock = tx->locks;
    while (current_lock) {
        lock *next_lock = current_lock->next;
        
        // 从全局锁列表中移除
        lock *prev_global_lock = NULL;
        lock *global_lock = manager->locks;
        while (global_lock) {
            if (global_lock->transaction_id == tx->id && global_lock->resource_id == current_lock->resource_id) {
                if (prev_global_lock) {
                    prev_global_lock->next = global_lock->next;
                } else {
                    manager->locks = global_lock->next;
                }
                free(global_lock);
                break;
            }
            prev_global_lock = global_lock;
            global_lock = global_lock->next;
        }
        
        free(current_lock);
        current_lock = next_lock;
    }
    tx->locks = NULL;
    
    // 应用撤销日志
    if (tx->undo_log) {
        // 撤销操作将在后续实现
        free(tx->undo_log);
        tx->undo_log = NULL;
    }
    
    // 清理重做日志
    if (tx->redo_log) {
        free(tx->redo_log);
        tx->redo_log = NULL;
    }
    
    // 更新事务状态
    tx->state = TRANSACTION_STATE_ROLLED_BACK;
    tx->commit_time = (uint64_t)time(NULL);
    manager->active_transactions--;
    
    return true;
}

// 获取事务状态
transaction_state transaction_get_state(transaction *tx) {
    if (!tx) {
        return TRANSACTION_STATE_FAILED;
    }
    return tx->state;
}

// 获取事务隔离级别
isolation_level transaction_get_isolation(transaction *tx) {
    if (!tx) {
        return ISOLATION_LEVEL_READ_COMMITTED;
    }
    return tx->isolation;
}

// 检查锁冲突
static bool check_lock_conflict(transaction_manager *manager, uint64_t resource_id, uint32_t transaction_id, lock_type type) {
    lock *current = manager->locks;
    while (current) {
        if (current->resource_id == resource_id && current->transaction_id != transaction_id) {
            // 检查锁类型冲突
            if ((type == LOCK_TYPE_EXCLUSIVE && current->type != LOCK_TYPE_NONE) ||
                (type == LOCK_TYPE_SHARED && current->type == LOCK_TYPE_EXCLUSIVE)) {
                return true;
            }
        }
        current = current->next;
    }
    return false;
}

// 加锁
bool transaction_lock(transaction_manager *manager, transaction *tx, uint64_t resource_id, lock_type type) {
    if (!manager || !tx || tx->state != TRANSACTION_STATE_ACTIVE) {
        return false;
    }
    
    // 检查锁冲突
    if (check_lock_conflict(manager, resource_id, tx->id, type)) {
        return false;
    }
    
    // 创建锁
    lock *new_lock = (lock *)malloc(sizeof(lock));
    if (!new_lock) {
        return false;
    }
    
    new_lock->resource_id = resource_id;
    new_lock->type = type;
    new_lock->transaction_id = tx->id;
    new_lock->acquire_time = (uint64_t)time(NULL);
    new_lock->next = NULL;
    
    // 添加到事务锁列表
    if (!tx->locks) {
        tx->locks = new_lock;
    } else {
        lock *current = tx->locks;
        while (current->next) {
            current = current->next;
        }
        current->next = new_lock;
    }
    
    // 添加到全局锁列表
    if (!manager->locks) {
        manager->locks = new_lock;
    } else {
        lock *current = manager->locks;
        while (current->next) {
            current = current->next;
        }
        current->next = new_lock;
    }
    
    return true;
}

// 解锁
bool transaction_unlock(transaction_manager *manager, transaction *tx, uint64_t resource_id) {
    if (!manager || !tx) {
        return false;
    }
    
    // 从事务锁列表中移除
    lock *prev_lock = NULL;
    lock *current_lock = tx->locks;
    while (current_lock) {
        if (current_lock->resource_id == resource_id) {
            if (prev_lock) {
                prev_lock->next = current_lock->next;
            } else {
                tx->locks = current_lock->next;
            }
            
            // 从全局锁列表中移除
            lock *prev_global_lock = NULL;
            lock *global_lock = manager->locks;
            while (global_lock) {
                if (global_lock->transaction_id == tx->id && global_lock->resource_id == resource_id) {
                    if (prev_global_lock) {
                        prev_global_lock->next = global_lock->next;
                    } else {
                        manager->locks = global_lock->next;
                    }
                    free(global_lock);
                    break;
                }
                prev_global_lock = global_lock;
                global_lock = global_lock->next;
            }
            
            free(current_lock);
            return true;
        }
        prev_lock = current_lock;
        current_lock = current_lock->next;
    }
    
    return false;
}

// 检查死锁
static bool detect_deadlock(transaction_manager *manager) {
    // 简化的死锁检测算法
    // 实际实现中应该使用等待图算法
    return false;
}

// 检查死锁
bool transaction_check_deadlock(transaction_manager *manager) {
    if (!manager) {
        return false;
    }
    return detect_deadlock(manager);
}

// 处理死锁
bool transaction_handle_deadlock(transaction_manager *manager) {
    if (!manager) {
        return false;
    }
    
    // 简化的死锁处理：选择最老的事务回滚
    transaction *oldest_tx = NULL;
    uint64_t oldest_time = (uint64_t)time(NULL) + 1;
    
    transaction *current = manager->transactions;
    while (current) {
        if (current->state == TRANSACTION_STATE_ACTIVE && current->start_time < oldest_time) {
            oldest_time = current->start_time;
            oldest_tx = current;
        }
        current = current->next;
    }
    
    if (oldest_tx) {
        return transaction_rollback(manager, oldest_tx);
    }
    
    return false;
}

// 获取活动事务数
uint32_t transaction_get_active_count(transaction_manager *manager) {
    if (!manager) {
        return 0;
    }
    return manager->active_transactions;
}

// 设置事务超时
bool transaction_set_timeout(transaction *tx, uint64_t timeout_ms) {
    // 简化实现：实际应该在事务管理器中跟踪超时
    (void)tx;
    (void)timeout_ms;
    return true;
}

// 清理超时事务
bool transaction_cleanup_timeout(transaction_manager *manager) {
    if (!manager) {
        return false;
    }
    
    uint64_t now = (uint64_t)time(NULL);
    bool cleaned = false;
    
    transaction *current = manager->transactions;
    while (current) {
        if (current->state == TRANSACTION_STATE_ACTIVE && 
            (now - current->start_time) > default_transaction_config.transaction_timeout / 1000) {
            transaction_rollback(manager, current);
            cleaned = true;
        }
        current = current->next;
    }
    
    return cleaned;
}

// 记录撤销日志
bool transaction_log_undo(transaction *tx, const void *data, uint32_t size) {
    if (!tx || !data) {
        return false;
    }
    
    // 简化实现：实际应该使用更复杂的日志结构
    if (!tx->undo_log) {
        tx->undo_log = malloc(size);
        if (!tx->undo_log) {
            return false;
        }
        memcpy(tx->undo_log, data, size);
    } else {
        void *new_log = realloc(tx->undo_log, size);
        if (!new_log) {
            return false;
        }
        tx->undo_log = new_log;
        memcpy(tx->undo_log, data, size);
    }
    
    return true;
}

// 记录重做日志
bool transaction_log_redo(transaction *tx, const void *data, uint32_t size) {
    if (!tx || !data) {
        return false;
    }
    
    // 简化实现：实际应该使用更复杂的日志结构
    if (!tx->redo_log) {
        tx->redo_log = malloc(size);
        if (!tx->redo_log) {
            return false;
        }
        memcpy(tx->redo_log, data, size);
    } else {
        void *new_log = realloc(tx->redo_log, size);
        if (!new_log) {
            return false;
        }
        tx->redo_log = new_log;
        memcpy(tx->redo_log, data, size);
    }
    
    return true;
}

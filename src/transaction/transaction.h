#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <stdint.h>
#include <stdbool.h>

// 事务状态
typedef enum {
    TRANSACTION_STATE_ACTIVE,
    TRANSACTION_STATE_COMMITTED,
    TRANSACTION_STATE_ROLLED_BACK,
    TRANSACTION_STATE_PREPARED,
    TRANSACTION_STATE_FAILED
} transaction_state;

// 事务隔离级别
typedef enum {
    ISOLATION_LEVEL_READ_UNCOMMITTED,
    ISOLATION_LEVEL_READ_COMMITTED,
    ISOLATION_LEVEL_REPEATABLE_READ,
    ISOLATION_LEVEL_SERIALIZABLE
} isolation_level;

// 锁类型
typedef enum {
    LOCK_TYPE_SHARED,
    LOCK_TYPE_EXCLUSIVE,
    LOCK_TYPE_INTENT_SHARED,
    LOCK_TYPE_INTENT_EXCLUSIVE
} lock_type;

// 锁模式
typedef enum {
    LOCK_MODE_NONE,
    LOCK_MODE_INTENT_SHARED,
    LOCK_MODE_SHARED,
    LOCK_MODE_INTENT_EXCLUSIVE,
    LOCK_MODE_SHARED_INTENT_EXCLUSIVE,
    LOCK_MODE_EXCLUSIVE
} lock_mode;

// 锁结构
typedef struct {
    uint64_t resource_id;
    lock_type type;
    uint32_t transaction_id;
    uint64_t acquire_time;
    struct lock *next;
} lock;

// 事务结构
typedef struct {
    uint32_t id;
    transaction_state state;
    isolation_level isolation;
    uint64_t start_time;
    uint64_t commit_time;
    lock *locks;
    void *undo_log;
    void *redo_log;
    struct transaction *next;
} transaction;

// 事务管理器结构
typedef struct {
    transaction *transactions;
    lock *locks;
    uint32_t next_transaction_id;
    uint32_t active_transactions;
    uint32_t max_transactions;
    bool initialized;
} transaction_manager;

// 事务配置结构
typedef struct {
    isolation_level default_isolation;
    uint32_t max_transactions;
    uint64_t transaction_timeout;
    bool enable_undo_log;
    bool enable_redo_log;
} transaction_config;

// 初始化事务管理器
transaction_manager *transaction_manager_init(const transaction_config *config);

// 销毁事务管理器
void transaction_manager_destroy(transaction_manager *manager);

// 开始事务
transaction *transaction_begin(transaction_manager *manager, isolation_level isolation);

// 提交事务
bool transaction_commit(transaction_manager *manager, transaction *tx);

// 回滚事务
bool transaction_rollback(transaction_manager *manager, transaction *tx);

// 获取事务状态
transaction_state transaction_get_state(transaction *tx);

// 获取事务隔离级别
isolation_level transaction_get_isolation(transaction *tx);

// 加锁
bool transaction_lock(transaction_manager *manager, transaction *tx, uint64_t resource_id, lock_type type);

// 解锁
bool transaction_unlock(transaction_manager *manager, transaction *tx, uint64_t resource_id);

// 检查死锁
bool transaction_check_deadlock(transaction_manager *manager);

// 处理死锁
bool transaction_handle_deadlock(transaction_manager *manager);

// 获取活动事务数
uint32_t transaction_get_active_count(transaction_manager *manager);

// 设置事务超时
bool transaction_set_timeout(transaction *tx, uint64_t timeout_ms);

// 清理超时事务
bool transaction_cleanup_timeout(transaction_manager *manager);

// 记录撤销日志
bool transaction_log_undo(transaction *tx, const void *data, uint32_t size);

// 记录重做日志
bool transaction_log_redo(transaction *tx, const void *data, uint32_t size);

#endif // TRANSACTION_H

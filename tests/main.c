#include "test.h"

#include "../src/config/config.h"
#include "../src/error/error.h"
#include "../src/logging/logging.h"
#include "../src/memory/memory_pool.h"
#include "../src/memory/memory_cache.h"
#include "../src/storage/storage_engine.h"
#include "../src/index/b_plus_tree.h"
#include "../src/security/security.h"
#include "../src/network/network.h"
#include "../src/transaction/transaction.h"
#include "../src/monitoring/monitoring.h"
#include "../src/backup/backup.h"
#include "../src/metadata/metadata.h"
#include "../src/audit/audit.h"
#include "../src/resource/resource.h"
#include "../src/optimizer/optimizer.h"
#include "../src/procedure/procedure.h"
#include "../src/replication/replication.h"
#include "../src/client/client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 配置测试
static int test_config_create(void) {
    config_system *config = config_init(NULL);
    int result = test_assert_not_null(config, "Failed to create config");
    if (config) {
        config_destroy(config);
    }
    return result;
}

static int test_config_get_int(void) {
    config_system *config = config_init(NULL);
    if (!config) {
        return ERROR_FAIL;
    }
    int value = config_get_int(config, "test.int_value", 42);
    int result = test_assert_equal(value, 42, "Failed to get int value");
    config_destroy(config);
    return result;
}

static int test_config_get_string(void) {
    config_system *config = config_init(NULL);
    if (!config) {
        return ERROR_FAIL;
    }
    const char *value = config_get_string(config, "test.string_value", "test");
    int result = test_assert_str_equal(value, "test", "Failed to get string value");
    config_destroy(config);
    return result;
}

// 错误测试
static int test_error_init(void) {
    error_system *error_sys = error_init(1024);
    int result = test_assert_not_null(error_sys, "Failed to initialize error system");
    if (error_sys) {
        error_destroy(error_sys);
    }
    return result;
}

// 日志测试
static int test_logging_init(void) {
    log_config config = {
        .log_path = "test.log",
        .min_level = LOG_LEVEL_INFO,
        .target = LOG_TARGET_FILE,
        .log_rotation = false,
        .max_log_size = 1024 * 1024,
        .max_log_files = 10
    };
    logging_system *logging = logging_init(&config);
    int result = test_assert_not_null(logging, "Failed to initialize logging system");
    if (logging) {
        logging_destroy(logging);
    }
    return result;
}

// 内存池测试
static int test_memory_pool_create(void) {
    config_system *config = config_init(NULL);
    if (!config) {
        return ERROR_FAIL;
    }
    MemoryPool *pool = memory_pool_init(config);
    int result = test_assert_not_null(pool, "Failed to create memory pool");
    if (pool) {
        memory_pool_destroy(pool);
    }
    config_destroy(config);
    return result;
}

// 内存缓存测试
static int test_memory_cache_create(void) {
    MemoryCache *cache = memory_cache_create(1024 * 1024, 100);
    int result = test_assert_not_null(cache, "Failed to create memory cache");
    if (cache) {
        memory_cache_destroy(cache);
    }
    return result;
}

// 存储引擎测试
static int test_storage_engine_create(void) {
    config_system *config = config_init(NULL);
    if (!config) {
        return ERROR_FAIL;
    }
    StorageEngineManager *storage = storage_engine_manager_init(config);
    int result = test_assert_not_null(storage, "Failed to create storage engine manager");
    if (storage) {
        storage_engine_manager_destroy(storage);
    }
    config_destroy(config);
    return result;
}

// B+树索引测试
static int test_b_plus_tree_create(void) {
    BPlusTree *tree = b_plus_tree_create(16);
    int result = test_assert_not_null(tree, "Failed to create B+ tree");
    if (tree) {
        b_plus_tree_destroy(tree);
    }
    return result;
}

// 安全测试
static int test_security_create(void) {
    security_system *security = security_init();
    int result = test_assert_not_null(security, "Failed to create security system");
    if (security) {
        security_destroy(security);
    }
    return result;
}

// 网络测试
static int test_network_create(void) {
    network_server *server = network_server_create(3306, 100);
    int result = test_assert_not_null(server, "Failed to create network server");
    if (server) {
        network_server_destroy(server);
    }
    return result;
}

// 事务测试
static int test_transaction_create(void) {
    transaction_manager *txn_manager = transaction_manager_create();
    int result = test_assert_not_null(txn_manager, "Failed to create transaction manager");
    if (txn_manager) {
        transaction_manager_destroy(txn_manager);
    }
    return result;
}

// 监控测试
static int test_monitoring_create(void) {
    monitoring_system *monitoring = monitoring_init();
    int result = test_assert_not_null(monitoring, "Failed to create monitoring system");
    if (monitoring) {
        monitoring_destroy(monitoring);
    }
    return result;
}

// 备份测试
static int test_backup_create(void) {
    BackupConfig *config = (BackupConfig *)malloc(sizeof(BackupConfig));
    if (!config) {
        return ERROR_FAIL;
    }
    config->backup_dir = "./backups";
    config->max_backups = 10;
    config->compress = false;
    config->compression_level = "6";
    config->encrypt = false;
    config->encryption_key = NULL;
    config->backup_type = BACKUP_TYPE_FULL;
    config->schedule = NULL;
    BackupManager *backup = backup_manager_init(config);
    int result = test_assert_not_null(backup, "Failed to create backup manager");
    if (backup) {
        backup_manager_destroy(backup);
    }
    free(config);
    return result;
}

// 元数据测试
static int test_metadata_create(void) {
    MetadataManager *metadata = metadata_manager_init("./metadata");
    int result = test_assert_not_null(metadata, "Failed to create metadata manager");
    if (metadata) {
        metadata_manager_destroy(metadata);
    }
    return result;
}

// 审计测试
static int test_audit_create(void) {
    AuditConfig *config = (AuditConfig *)malloc(sizeof(AuditConfig));
    if (!config) {
        return ERROR_FAIL;
    }
    config->enabled = true;
    config->log_dir = "./audit";
    config->log_file = "audit";
    config->log_format = AUDIT_FORMAT_TEXT;
    config->max_log_size = 100;
    config->max_log_files = 10;
    config->rotate = true;
    config->compress = false;
    config->encrypt = false;
    config->encryption_key = NULL;
    config->log_login = true;
    config->log_logout = true;
    config->log_query = true;
    config->log_dml = true;
    config->log_ddl = true;
    config->log_admin = true;
    config->log_error = true;
    config->min_query_length = 0;
    config->max_query_length = 10240;
    AuditManager *audit = audit_manager_init(config);
    int result = test_assert_not_null(audit, "Failed to create audit manager");
    if (audit) {
        audit_manager_destroy(audit);
    }
    free(config);
    return result;
}

// 资源管理测试
static int test_resource_create(void) {
    config_system *config = config_init(NULL);
    if (!config) {
        return ERROR_FAIL;
    }
    resource_manager_t *resource = resource_manager_create(config);
    int result = test_assert_not_null(resource, "Failed to create resource manager");
    if (resource) {
        resource_manager_destroy(resource);
    }
    config_destroy(config);
    return result;
}

// 优化器测试
static int test_optimizer_create(void) {
    config_system *config = config_init(NULL);
    if (!config) {
        return ERROR_FAIL;
    }
    MetadataManager *metadata = metadata_manager_init("./metadata");
    if (!metadata) {
        config_destroy(config);
        return ERROR_FAIL;
    }
    query_optimizer *optimizer = optimizer_create(config, metadata);
    int result = test_assert_not_null(optimizer, "Failed to create optimizer");
    if (optimizer) {
        optimizer_destroy(optimizer);
    }
    metadata_manager_destroy(metadata);
    config_destroy(config);
    return result;
}

// 存储过程测试
static int test_procedure_create(void) {
    config_system *config = config_init(NULL);
    if (!config) {
        return ERROR_FAIL;
    }
    MetadataManager *metadata = metadata_manager_init("./metadata");
    if (!metadata) {
        config_destroy(config);
        return ERROR_FAIL;
    }
    procedure_manager *procedure = procedure_manager_create(config, metadata);
    int result = test_assert_not_null(procedure, "Failed to create procedure manager");
    if (procedure) {
        procedure_manager_destroy(procedure);
    }
    metadata_manager_destroy(metadata);
    config_destroy(config);
    return result;
}

// 复制测试
static int test_replication_create(void) {
    config_system *config = config_init(NULL);
    if (!config) {
        return ERROR_FAIL;
    }
    network_server *server = network_server_create(3306, 100);
    if (!server) {
        config_destroy(config);
        return ERROR_FAIL;
    }
    replication_manager *replication = replication_manager_create(config, server);
    int result = test_assert_not_null(replication, "Failed to create replication manager");
    if (replication) {
        replication_manager_destroy(replication);
    }
    network_server_destroy(server);
    config_destroy(config);
    return result;
}

// 客户端测试
static int test_client_create(void) {
    client_config config = {
        .host = "localhost",
        .port = 3306,
        .user = "test",
        .password = "test",
        .database = "test",
        .default_character_set = "utf8",
        .interactive = false,
        .quiet = true,
        .batch = false,
        .batch_file = NULL,
        .prompt = "test>",
        .command_history_size = 100
    };
    client *cli = client_create(&config);
    int result = test_assert_not_null(cli, "Failed to create client");
    if (cli) {
        client_destroy(cli);
    }
    return result;
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool quiet = false;
    bool exit_on_fail = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            quiet = true;
        } else if (strcmp(argv[i], "--exit-on-fail") == 0 || strcmp(argv[i], "-e") == 0) {
            exit_on_fail = true;
        }
    }

    test_runner *runner = test_runner_create(verbose, quiet, exit_on_fail);
    if (!runner) {
        fprintf(stderr, "Failed to create test runner\n");
        return 1;
    }

    // 配置测试
    test_suite *config_suite = test_runner_add_suite(runner, "Config");
    test_suite_add_test(config_suite, "create", test_config_create);
    test_suite_add_test(config_suite, "get_int", test_config_get_int);
    test_suite_add_test(config_suite, "get_string", test_config_get_string);

    // 错误测试
    test_suite *error_suite = test_runner_add_suite(runner, "Error");
    test_suite_add_test(error_suite, "init", test_error_init);

    // 日志测试
    test_suite *logging_suite = test_runner_add_suite(runner, "Logging");
    test_suite_add_test(logging_suite, "init", test_logging_init);

    // 内存测试
    test_suite *memory_suite = test_runner_add_suite(runner, "Memory");
    test_suite_add_test(memory_suite, "pool_create", test_memory_pool_create);
    test_suite_add_test(memory_suite, "cache_create", test_memory_cache_create);

    // 存储测试
    test_suite *storage_suite = test_runner_add_suite(runner, "Storage");
    test_suite_add_test(storage_suite, "create", test_storage_engine_create);

    // 索引测试
    test_suite *index_suite = test_runner_add_suite(runner, "Index");
    test_suite_add_test(index_suite, "b_plus_tree_create", test_b_plus_tree_create);

    // 安全测试
    test_suite *security_suite = test_runner_add_suite(runner, "Security");
    test_suite_add_test(security_suite, "create", test_security_create);

    // 网络测试
    test_suite *network_suite = test_runner_add_suite(runner, "Network");
    test_suite_add_test(network_suite, "create", test_network_create);

    // 事务测试
    test_suite *transaction_suite = test_runner_add_suite(runner, "Transaction");
    test_suite_add_test(transaction_suite, "create", test_transaction_create);

    // 监控测试
    test_suite *monitoring_suite = test_runner_add_suite(runner, "Monitoring");
    test_suite_add_test(monitoring_suite, "create", test_monitoring_create);

    // 备份测试
    test_suite *backup_suite = test_runner_add_suite(runner, "Backup");
    test_suite_add_test(backup_suite, "create", test_backup_create);

    // 元数据测试
    test_suite *metadata_suite = test_runner_add_suite(runner, "Metadata");
    test_suite_add_test(metadata_suite, "create", test_metadata_create);

    // 审计测试
    test_suite *audit_suite = test_runner_add_suite(runner, "Audit");
    test_suite_add_test(audit_suite, "create", test_audit_create);

    // 资源测试
    test_suite *resource_suite = test_runner_add_suite(runner, "Resource");
    test_suite_add_test(resource_suite, "create", test_resource_create);

    // 优化器测试
    test_suite *optimizer_suite = test_runner_add_suite(runner, "Optimizer");
    test_suite_add_test(optimizer_suite, "create", test_optimizer_create);

    // 存储过程测试
    test_suite *procedure_suite = test_runner_add_suite(runner, "Procedure");
    test_suite_add_test(procedure_suite, "create", test_procedure_create);

    // 复制测试
    test_suite *replication_suite = test_runner_add_suite(runner, "Replication");
    test_suite_add_test(replication_suite, "create", test_replication_create);

    // 客户端测试
    test_suite *client_suite = test_runner_add_suite(runner, "Client");
    test_suite_add_test(client_suite, "create", test_client_create);

    // 运行测试
    int result = test_runner_run(runner);

    test_runner_destroy(runner);

    return result == SUCCESS ? 0 : 1;
}

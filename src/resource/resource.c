#include "resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

resource_manager_t *resource_manager_create(config_t *config) {
    resource_manager_t *rm = (resource_manager_t *)malloc(sizeof(resource_manager_t));
    if (!rm) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_RESOURCE, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for resource manager");
        return NULL;
    }

    memset(rm, 0, sizeof(resource_manager_t));

    int memory_limit = config_get_int(config, "resource", "memory_limit_mb", 1024);
    int connection_limit = config_get_int(config, "resource", "connection_limit", 100);
    int query_limit = config_get_int(config, "resource", "query_limit_per_second", 1000);
    int transaction_limit = config_get_int(config, "resource", "transaction_limit", 100);
    int disk_limit = config_get_int(config, "resource", "disk_limit_gb", 100);
    int cpu_limit = config_get_int(config, "resource", "cpu_limit_percent", 80);

    resource_manager_add_limit(rm, RESOURCE_TYPE_MEMORY, RESOURCE_LIMIT_TYPE_HARD, memory_limit * 1024 * 1024);
    resource_manager_add_limit(rm, RESOURCE_TYPE_CONNECTION, RESOURCE_LIMIT_TYPE_HARD, connection_limit);
    resource_manager_add_limit(rm, RESOURCE_TYPE_QUERY, RESOURCE_LIMIT_TYPE_HARD, query_limit);
    resource_manager_add_limit(rm, RESOURCE_TYPE_TRANSACTION, RESOURCE_LIMIT_TYPE_HARD, transaction_limit);
    resource_manager_add_limit(rm, RESOURCE_TYPE_DISK, RESOURCE_LIMIT_TYPE_HARD, disk_limit * 1024 * 1024 * 1024);
    resource_manager_add_limit(rm, RESOURCE_TYPE_CPU, RESOURCE_LIMIT_TYPE_HARD, cpu_limit);

    resource_manager_add_limit(rm, RESOURCE_TYPE_MEMORY, RESOURCE_LIMIT_TYPE_SOFT, (memory_limit * 1024 * 1024) * 0.8);
    resource_manager_add_limit(rm, RESOURCE_TYPE_CONNECTION, RESOURCE_LIMIT_TYPE_SOFT, connection_limit * 0.8);
    resource_manager_add_limit(rm, RESOURCE_TYPE_QUERY, RESOURCE_LIMIT_TYPE_SOFT, query_limit * 0.8);
    resource_manager_add_limit(rm, RESOURCE_TYPE_TRANSACTION, RESOURCE_LIMIT_TYPE_SOFT, transaction_limit * 0.8);
    resource_manager_add_limit(rm, RESOURCE_TYPE_DISK, RESOURCE_LIMIT_TYPE_SOFT, (disk_limit * 1024 * 1024 * 1024) * 0.8);
    resource_manager_add_limit(rm, RESOURCE_TYPE_CPU, RESOURCE_LIMIT_TYPE_SOFT, cpu_limit * 0.8);

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_RESOURCE, "Resource manager created with default limits");
    return rm;
}

void resource_manager_destroy(resource_manager_t *rm) {
    if (rm) {
        free(rm);
    }
}

int resource_manager_add_limit(resource_manager_t *rm, int resource_type, int limit_type, unsigned long long value) {
    if (rm->limit_count >= MAX_RESOURCE_LIMITS) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_RESOURCE, ERROR_LIMIT_EXCEEDED, "Maximum number of resource limits reached");
        return ERROR_LIMIT_EXCEEDED;
    }

    resource_limit_t *limit = &rm->limits[rm->limit_count];
    limit->resource_type = resource_type;
    limit->limit_type = limit_type;
    limit->value = value;
    limit->current_usage = 0;

    rm->limit_count++;
    return SUCCESS;
}

int resource_manager_check_limit(resource_manager_t *rm, int resource_type, unsigned long long usage) {
    for (int i = 0; i < rm->limit_count; i++) {
        resource_limit_t *limit = &rm->limits[i];
        if (limit->resource_type == resource_type) {
            unsigned long long new_usage = 0;
            switch (resource_type) {
                case RESOURCE_TYPE_MEMORY:
                    new_usage = rm->total_memory_usage + usage;
                    break;
                case RESOURCE_TYPE_CONNECTION:
                    new_usage = rm->total_connections + usage;
                    break;
                case RESOURCE_TYPE_QUERY:
                    new_usage = rm->total_queries + usage;
                    break;
                case RESOURCE_TYPE_TRANSACTION:
                    new_usage = rm->total_transactions + usage;
                    break;
                case RESOURCE_TYPE_DISK:
                    new_usage = rm->disk_usage + usage;
                    break;
                case RESOURCE_TYPE_CPU:
                    new_usage = rm->cpu_usage_percent + usage;
                    break;
                default:
                    return ERROR_INVALID_PARAMETER;
            }

            if (limit->limit_type == RESOURCE_LIMIT_TYPE_HARD && new_usage > limit->value) {
                return ERROR_LIMIT_EXCEEDED;
            } else if (limit->limit_type == RESOURCE_LIMIT_TYPE_SOFT && new_usage > limit->value) {
                logging_log(LOG_LEVEL_WARNING, LOGGING_MODULE_RESOURCE, "Soft limit exceeded for resource type %d", resource_type);
            }
        }
    }

    return SUCCESS;
}

int resource_manager_update_usage(resource_manager_t *rm, int resource_type, unsigned long long usage_delta) {
    switch (resource_type) {
        case RESOURCE_TYPE_MEMORY:
            rm->total_memory_usage += usage_delta;
            break;
        case RESOURCE_TYPE_CONNECTION:
            rm->total_connections += usage_delta;
            break;
        case RESOURCE_TYPE_QUERY:
            rm->total_queries += usage_delta;
            break;
        case RESOURCE_TYPE_TRANSACTION:
            rm->total_transactions += usage_delta;
            break;
        case RESOURCE_TYPE_DISK:
            rm->disk_usage += usage_delta;
            break;
        case RESOURCE_TYPE_CPU:
            rm->cpu_usage_percent += usage_delta;
            break;
        default:
            return ERROR_INVALID_PARAMETER;
    }

    return SUCCESS;
}

unsigned long long resource_manager_get_memory_usage(resource_manager_t *rm) {
    return rm->total_memory_usage;
}

int resource_manager_get_connection_count(resource_manager_t *rm) {
    return rm->total_connections;
}

int resource_manager_get_query_count(resource_manager_t *rm) {
    return rm->total_queries;
}

int resource_manager_get_transaction_count(resource_manager_t *rm) {
    return rm->total_transactions;
}

unsigned long long resource_manager_get_disk_usage(resource_manager_t *rm) {
    return rm->disk_usage;
}

int resource_manager_get_cpu_usage(resource_manager_t *rm) {
    return rm->cpu_usage_percent;
}

void resource_manager_print_stats(resource_manager_t *rm) {
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_RESOURCE, "Resource Usage Stats:");
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_RESOURCE, "Memory: %llu bytes", rm->total_memory_usage);
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_RESOURCE, "Connections: %d", rm->total_connections);
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_RESOURCE, "Queries: %d", rm->total_queries);
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_RESOURCE, "Transactions: %d", rm->total_transactions);
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_RESOURCE, "Disk: %llu bytes", rm->disk_usage);
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_RESOURCE, "CPU: %d%%", rm->cpu_usage_percent);
}

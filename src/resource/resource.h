#ifndef RESOURCE_H
#define RESOURCE_H

#include "../config/config.h"
#include "../error/error.h"
#include "../logging/logging.h"

#define RESOURCE_MODULE "resource"

#define MAX_RESOURCE_LIMITS 64

#define RESOURCE_TYPE_MEMORY 1
#define RESOURCE_TYPE_CONNECTION 2
#define RESOURCE_TYPE_QUERY 3
#define RESOURCE_TYPE_TRANSACTION 4
#define RESOURCE_TYPE_DISK 5
#define RESOURCE_TYPE_CPU 6

#define RESOURCE_LIMIT_TYPE_HARD 1
#define RESOURCE_LIMIT_TYPE_SOFT 2

typedef struct {
    int resource_type;
    int limit_type;
    unsigned long long value;
    unsigned long long current_usage;
} resource_limit_t;

typedef struct {
    resource_limit_t limits[MAX_RESOURCE_LIMITS];
    int limit_count;
    unsigned long long total_memory_usage;
    int total_connections;
    int total_queries;
    int total_transactions;
    unsigned long long disk_usage;
    int cpu_usage_percent;
} resource_manager_t;

resource_manager_t *resource_manager_create(config_t *config);
void resource_manager_destroy(resource_manager_t *rm);

int resource_manager_add_limit(resource_manager_t *rm, int resource_type, int limit_type, unsigned long long value);
int resource_manager_check_limit(resource_manager_t *rm, int resource_type, unsigned long long usage);
int resource_manager_update_usage(resource_manager_t *rm, int resource_type, unsigned long long usage_delta);

unsigned long long resource_manager_get_memory_usage(resource_manager_t *rm);
int resource_manager_get_connection_count(resource_manager_t *rm);
int resource_manager_get_query_count(resource_manager_t *rm);
int resource_manager_get_transaction_count(resource_manager_t *rm);
unsigned long long resource_manager_get_disk_usage(resource_manager_t *rm);
int resource_manager_get_cpu_usage(resource_manager_t *rm);

void resource_manager_print_stats(resource_manager_t *rm);

#endif
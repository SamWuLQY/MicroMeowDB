#include "optimizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

query_optimizer *optimizer_create(config_t *config, MetadataManager *metadata) {
    query_optimizer *optimizer = (query_optimizer *)malloc(sizeof(query_optimizer));
    if (!optimizer) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_OPTIMIZER, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for query optimizer");
        return NULL;
    }

    optimizer->config = config;
    optimizer->metadata = metadata;
    optimizer->statistics_cache = NULL;
    optimizer->statistics_count = 0;
    optimizer->optimization_level = config_get_int(config, "optimizer", "optimization_level", OPTIMIZATION_LEVEL_FULL);
    optimizer->use_statistics = config_get_bool(config, "optimizer", "use_statistics", true);
    optimizer->enable_join_reordering = config_get_bool(config, "optimizer", "enable_join_reordering", true);
    optimizer->enable_index_selection = config_get_bool(config, "optimizer", "enable_index_selection", true);
    optimizer->enable_predicate_pushdown = config_get_bool(config, "optimizer", "enable_predicate_pushdown", true);
    optimizer->enable_projection_pruning = config_get_bool(config, "optimizer", "enable_projection_pruning", true);

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_OPTIMIZER, "Query optimizer created with optimization level %d", optimizer->optimization_level);
    return optimizer;
}

void optimizer_destroy(query_optimizer *optimizer) {
    if (optimizer) {
        if (optimizer->statistics_cache) {
            free(optimizer->statistics_cache);
        }
        free(optimizer);
    }
}

query *optimizer_parse_query(query_optimizer *optimizer, const char *query_text) {
    if (!optimizer || !query_text) {
        return NULL;
    }

    query *q = (query *)malloc(sizeof(query));
    if (!q) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_OPTIMIZER, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for query");
        return NULL;
    }

    memset(q, 0, sizeof(query));

    char *query_copy = strdup(query_text);
    if (!query_copy) {
        free(q);
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_OPTIMIZER, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for query copy");
        return NULL;
    }

    char *token = strtok(query_copy, " ");
    if (token) {
        if (strcasecmp(token, "SELECT") == 0) {
            q->type = QUERY_TYPE_SELECT;
        } else if (strcasecmp(token, "INSERT") == 0) {
            q->type = QUERY_TYPE_INSERT;
        } else if (strcasecmp(token, "UPDATE") == 0) {
            q->type = QUERY_TYPE_UPDATE;
        } else if (strcasecmp(token, "DELETE") == 0) {
            q->type = QUERY_TYPE_DELETE;
        } else if (strcasecmp(token, "CREATE") == 0) {
            q->type = QUERY_TYPE_CREATE;
        } else if (strcasecmp(token, "DROP") == 0) {
            q->type = QUERY_TYPE_DROP;
        } else if (strcasecmp(token, "ALTER") == 0) {
            q->type = QUERY_TYPE_ALTER;
        } else if (strcasecmp(token, "TRUNCATE") == 0) {
            q->type = QUERY_TYPE_TRUNCATE;
        } else if (strcasecmp(token, "RENAME") == 0) {
            q->type = QUERY_TYPE_RENAME;
        } else if (strcasecmp(token, "GRANT") == 0) {
            q->type = QUERY_TYPE_GRANT;
        } else if (strcasecmp(token, "REVOKE") == 0) {
            q->type = QUERY_TYPE_REVOKE;
        } else if (strcasecmp(token, "COMMIT") == 0) {
            q->type = QUERY_TYPE_COMMIT;
        } else if (strcasecmp(token, "ROLLBACK") == 0) {
            q->type = QUERY_TYPE_ROLLBACK;
        } else if (strcasecmp(token, "BEGIN") == 0) {
            q->type = QUERY_TYPE_BEGIN;
        }
    }

    free(query_copy);
    return q;
}

void optimizer_free_query(query *q) {
    if (q) {
        for (int i = 0; i < q->table_count; i++) {
            if (q->tables[i]) {
                if (q->tables[i]->name) free(q->tables[i]->name);
                if (q->tables[i]->alias) free(q->tables[i]->alias);
                if (q->tables[i]->schema) free(q->tables[i]->schema);
                free(q->tables[i]);
            }
        }
        for (int i = 0; i < q->column_count; i++) {
            if (q->columns[i]) free(q->columns[i]);
        }
        for (int i = 0; i < q->predicate_count; i++) {
            if (q->predicates[i]) {
                if (q->predicates[i]->column) free(q->predicates[i]->column);
                if (q->predicates[i]->expr) {
                    if (q->predicates[i]->expr->value) free(q->predicates[i]->expr->value);
                    if (q->predicates[i]->expr->left) free(q->predicates[i]->expr->left);
                    if (q->predicates[i]->expr->right) free(q->predicates[i]->expr->right);
                    free(q->predicates[i]->expr);
                }
                free(q->predicates[i]);
            }
        }
        for (int i = 0; i < q->join_count; i++) {
            if (q->joins[i]) {
                if (q->joins[i]->condition) {
                    if (q->joins[i]->condition->column) free(q->joins[i]->condition->column);
                    if (q->joins[i]->condition->expr) {
                        if (q->joins[i]->condition->expr->value) free(q->joins[i]->condition->expr->value);
                        if (q->joins[i]->condition->expr->left) free(q->joins[i]->condition->expr->left);
                        if (q->joins[i]->condition->expr->right) free(q->joins[i]->condition->expr->right);
                        free(q->joins[i]->condition->expr);
                    }
                    free(q->joins[i]->condition);
                }
                free(q->joins[i]);
            }
        }
        for (int i = 0; i < q->group_by_count; i++) {
            if (q->group_by_columns[i]) free(q->group_by_columns[i]);
        }
        for (int i = 0; i < q->order_by_count; i++) {
            if (q->order_by_columns[i]) free(q->order_by_columns[i]);
        }
        for (int i = 0; i < q->insert_value_count; i++) {
            if (q->insert_values[i]) free(q->insert_values[i]);
        }
        for (int i = 0; i < q->update_value_count; i++) {
            if (q->update_values[i]) free(q->update_values[i]);
        }
        if (q->create_definition) free(q->create_definition);
        if (q->drop_target) free(q->drop_target);
        if (q->alter_operation) free(q->alter_operation);
        if (q->truncate_target) free(q->truncate_target);
        if (q->rename_old_name) free(q->rename_old_name);
        if (q->rename_new_name) free(q->rename_new_name);
        if (q->grant_privileges) free(q->grant_privileges);
        if (q->grant_user) free(q->grant_user);
        if (q->revoke_privileges) free(q->revoke_privileges);
        if (q->revoke_user) free(q->revoke_user);
        free(q);
    }
}

plan_node *create_plan_node(int type, const char *name) {
    plan_node *node = (plan_node *)malloc(sizeof(plan_node));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(plan_node));
    node->type = type;
    if (name) {
        node->name = strdup(name);
    }

    return node;
}

query_plan *optimizer_create_plan(query_optimizer *optimizer, query *q) {
    if (!optimizer || !q) {
        return NULL;
    }

    query_plan *plan = (query_plan *)malloc(sizeof(query_plan));
    if (!plan) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_OPTIMIZER, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for query plan");
        return NULL;
    }

    memset(plan, 0, sizeof(query_plan));

    switch (q->type) {
        case QUERY_TYPE_SELECT:
            {
                if (q->table_count > 0) {
                    plan_node *scan_node = create_plan_node(PLAN_NODE_TYPE_SEQUENTIAL_SCAN, "Sequential Scan");
                    if (scan_node) {
                        scan_node->table = q->tables[0];
                        plan->root = scan_node;
                        plan->node_count++;
                    }
                }
            }
            break;
        default:
            break;
    }

    return plan;
}

void optimizer_free_plan(query_plan *plan) {
    if (plan) {
        if (plan->root) {
            plan_node *current = plan->root;
            while (current) {
                plan_node *next = current->next;
                if (current->name) free(current->name);
                if (current->index_name) free(current->index_name);
                if (current->filter) {
                    if (current->filter->column) free(current->filter->column);
                    if (current->filter->expr) {
                        if (current->filter->expr->value) free(current->filter->expr->value);
                        if (current->filter->expr->left) free(current->filter->expr->left);
                        if (current->filter->expr->right) free(current->filter->expr->right);
                        free(current->filter->expr);
                    }
                    free(current->filter);
                }
                for (int i = 0; i < current->column_count; i++) {
                    if (current->columns[i]) free(current->columns[i]);
                }
                for (int i = 0; i < current->sort_column_count; i++) {
                    if (current->sort_columns[i]) free(current->sort_columns[i]);
                }
                if (current->join_condition) {
                    if (current->join_condition->column) free(current->join_condition->column);
                    if (current->join_condition->expr) {
                        if (current->join_condition->expr->value) free(current->join_condition->expr->value);
                        if (current->join_condition->expr->left) free(current->join_condition->expr->left);
                        if (current->join_condition->expr->right) free(current->join_condition->expr->right);
                        free(current->join_condition->expr);
                    }
                    free(current->join_condition);
                }
                free(current);
                current = next;
            }
        }
        if (plan->query_text) free(plan->query_text);
        free(plan);
    }
}

query_plan *optimizer_optimize_plan(query_optimizer *optimizer, query_plan *plan) {
    if (!optimizer || !plan) {
        return plan;
    }

    if (optimizer->optimization_level == OPTIMIZATION_LEVEL_NONE) {
        return plan;
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_OPTIMIZER, "Optimizing query plan with level %d", optimizer->optimization_level);

    return plan;
}

int optimizer_execute_plan(query_optimizer *optimizer, query_plan *plan, void **result) {
    if (!optimizer || !plan) {
        return ERROR_INVALID_PARAMETER;
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_OPTIMIZER, "Executing query plan");

    return SUCCESS;
}

int optimizer_update_statistics(query_optimizer *optimizer, const char *table_name, const char *column_name, int type, double value, const char *string_value) {
    if (!optimizer || !table_name) {
        return ERROR_INVALID_PARAMETER;
    }

    statistics *stat = (statistics *)malloc(sizeof(statistics));
    if (!stat) {
        return ERROR_OUT_OF_MEMORY;
    }

    stat->table_name = strdup(table_name);
    if (column_name) {
        stat->column_name = strdup(column_name);
    }
    stat->type = type;
    stat->value = value;
    if (string_value) {
        stat->string_value = strdup(string_value);
    }
    stat->timestamp = (uint64_t)time(NULL);

    optimizer->statistics_cache = (statistics *)realloc(optimizer->statistics_cache, (optimizer->statistics_count + 1) * sizeof(statistics));
    if (!optimizer->statistics_cache) {
        free(stat);
        return ERROR_OUT_OF_MEMORY;
    }

    optimizer->statistics_cache[optimizer->statistics_count++] = *stat;

    return SUCCESS;
}

statistics *optimizer_get_statistics(query_optimizer *optimizer, const char *table_name, const char *column_name, int type) {
    if (!optimizer || !table_name) {
        return NULL;
    }

    for (int i = 0; i < optimizer->statistics_count; i++) {
        statistics *stat = &optimizer->statistics_cache[i];
        if (strcmp(stat->table_name, table_name) == 0) {
            if ((!column_name || (stat->column_name && strcmp(stat->column_name, column_name) == 0)) && stat->type == type) {
                return stat;
            }
        }
    }

    return NULL;
}

void optimizer_print_plan(query_optimizer *optimizer, query_plan *plan) {
    if (!optimizer || !plan) {
        return;
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_OPTIMIZER, "Query Plan:");
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_OPTIMIZER, "Total Cost: %f", plan->total_cost);
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_OPTIMIZER, "Estimated Rows: %f", plan->estimated_rows);

    plan_node *current = plan->root;
    while (current) {
        const char *node_type_str = "Unknown";
        switch (current->type) {
            case PLAN_NODE_TYPE_SEQUENTIAL_SCAN:
                node_type_str = "Sequential Scan";
                break;
            case PLAN_NODE_TYPE_INDEX_SCAN:
                node_type_str = "Index Scan";
                break;
            case PLAN_NODE_TYPE_JOIN:
                node_type_str = "Join";
                break;
            case PLAN_NODE_TYPE_FILTER:
                node_type_str = "Filter";
                break;
            case PLAN_NODE_TYPE_PROJECTION:
                node_type_str = "Projection";
                break;
            case PLAN_NODE_TYPE_SORT:
                node_type_str = "Sort";
                break;
            case PLAN_NODE_TYPE_AGGREGATE:
                node_type_str = "Aggregate";
                break;
            case PLAN_NODE_TYPE_LIMIT:
                node_type_str = "Limit";
                break;
            case PLAN_NODE_TYPE_OFFSET:
                node_type_str = "Offset";
                break;
        }

        logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_OPTIMIZER, "Node: %s, Cost: %f, Rows: %f", node_type_str, current->estimated_cost, current->estimated_rows);
        current = current->next;
    }
}

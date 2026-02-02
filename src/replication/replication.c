#include "replication.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

static binlog_manager *binlog_manager_create(config_t *config);
static void binlog_manager_destroy(binlog_manager *bm);
static int binlog_manager_open(binlog_manager *bm);
static int binlog_manager_write(binlog_manager *bm, binlog_event *event);
static int binlog_manager_rotate(binlog_manager *bm);

replication_manager *replication_manager_create(config_t *config, network_server *server) {
    if (!config) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_REPLICATION, ERROR_INVALID_PARAMETER, "Invalid configuration");
        return NULL;
    }

    replication_manager *rm = (replication_manager *)malloc(sizeof(replication_manager));
    if (!rm) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_REPLICATION, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for replication manager");
        return NULL;
    }

    memset(rm, 0, sizeof(replication_manager));
    rm->config = config;
    rm->server = server;
    rm->role = config_get_int(config, "replication", "role", REPLICATION_ROLE_MASTER);
    rm->server_id = strdup(config_get_string(config, "replication", "server_id", "1"));
    rm->master_host = strdup(config_get_string(config, "replication", "master_host", "localhost"));
    rm->master_port = config_get_int(config, "replication", "master_port", 3306);
    rm->master_user = strdup(config_get_string(config, "replication", "master_user", "repl"));
    rm->master_password = strdup(config_get_string(config, "replication", "master_password", "repl"));
    rm->replicate_do_db = strdup(config_get_string(config, "replication", "replicate_do_db", ""));
    rm->replicate_ignore_db = strdup(config_get_string(config, "replication", "replicate_ignore_db", ""));
    rm->replicate_do_table = strdup(config_get_string(config, "replication", "replicate_do_table", ""));
    rm->replicate_ignore_table = strdup(config_get_string(config, "replication", "replicate_ignore_table", ""));
    rm->read_only = config_get_bool(config, "replication", "read_only", false);
    rm->sync_type = config_get_int(config, "replication", "sync_type", SYNC_TYPE_INCREMENTAL);

    rm->binlog_manager = binlog_manager_create(config);
    if (!rm->binlog_manager) {
        free(rm);
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_REPLICATION, ERROR_OPERATION_FAILED, "Failed to create binlog manager");
        return NULL;
    }

    rm->slave_conns = (replication_connection *)malloc(MAX_REPLICAS * sizeof(replication_connection));
    if (!rm->slave_conns) {
        binlog_manager_destroy(rm->binlog_manager);
        free(rm);
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_REPLICATION, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for slave connections");
        return NULL;
    }

    memset(rm->slave_conns, 0, MAX_REPLICAS * sizeof(replication_connection));
    rm->slave_count = 0;
    rm->state = REPLICATION_STATE_INIT;
    rm->initialized = true;
    rm->running = false;

    pthread_mutex_init(&rm->mutex, NULL);
    pthread_cond_init(&rm->cond, NULL);

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Replication manager created with role %s", rm->role == REPLICATION_ROLE_MASTER ? "Master" : rm->role == REPLICATION_ROLE_SLAVE ? "Slave" : "Both");
    return rm;
}

void replication_manager_destroy(replication_manager *rm) {
    if (rm) {
        if (rm->running) {
            replication_manager_stop(rm);
        }

        if (rm->binlog_manager) {
            binlog_manager_destroy(rm->binlog_manager);
        }

        if (rm->master_conn) {
            if (rm->master_conn->sockfd > 0) {
                close(rm->master_conn->sockfd);
            }
            if (rm->master_conn->host) free(rm->master_conn->host);
            if (rm->master_conn->user) free(rm->master_conn->user);
            if (rm->master_conn->password) free(rm->master_conn->password);
            if (rm->master_conn->binlog_filename) free(rm->master_conn->binlog_filename);
            free(rm->master_conn);
        }

        for (int i = 0; i < rm->slave_count; i++) {
            replication_connection *slave = &rm->slave_conns[i];
            if (slave->sockfd > 0) {
                close(slave->sockfd);
            }
            if (slave->host) free(slave->host);
            if (slave->user) free(slave->user);
            if (slave->password) free(slave->password);
            if (slave->binlog_filename) free(slave->binlog_filename);
        }

        if (rm->slave_conns) {
            free(rm->slave_conns);
        }

        if (rm->server_id) free(rm->server_id);
        if (rm->master_host) free(rm->master_host);
        if (rm->master_user) free(rm->master_user);
        if (rm->master_password) free(rm->master_password);
        if (rm->replicate_do_db) free(rm->replicate_do_db);
        if (rm->replicate_ignore_db) free(rm->replicate_ignore_db);
        if (rm->replicate_do_table) free(rm->replicate_do_table);
        if (rm->replicate_ignore_table) free(rm->replicate_ignore_table);

        pthread_mutex_destroy(&rm->mutex);
        pthread_cond_destroy(&rm->cond);

        free(rm);
    }
}

int replication_manager_start(replication_manager *rm) {
    if (!rm || !rm->initialized) {
        return ERROR_INVALID_PARAMETER;
    }

    if (rm->running) {
        return SUCCESS;
    }

    rm->running = true;
    rm->state = REPLICATION_STATE_CONNECTING;

    if (rm->role == REPLICATION_ROLE_MASTER || rm->role == REPLICATION_ROLE_BOTH) {
        int ret = binlog_manager_open(rm->binlog_manager);
        if (ret != SUCCESS) {
            rm->state = REPLICATION_STATE_ERROR;
            error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_REPLICATION, ret, "Failed to open binlog manager");
            return ret;
        }
    }

    if (rm->role == REPLICATION_ROLE_SLAVE || rm->role == REPLICATION_ROLE_BOTH) {
        int ret = replication_manager_connect_to_master(rm, rm->master_host, rm->master_port, rm->master_user, rm->master_password);
        if (ret != SUCCESS) {
            rm->state = REPLICATION_STATE_ERROR;
            error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_REPLICATION, ret, "Failed to connect to master");
            return ret;
        }
    }

    rm->state = REPLICATION_STATE_RUNNING;
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Replication manager started successfully");
    return SUCCESS;
}

int replication_manager_stop(replication_manager *rm) {
    if (!rm || !rm->initialized) {
        return ERROR_INVALID_PARAMETER;
    }

    if (!rm->running) {
        return SUCCESS;
    }

    rm->running = false;
    rm->state = REPLICATION_STATE_STOPPED;

    if (rm->master_conn && rm->master_conn->running) {
        replication_manager_disconnect_from_master(rm);
    }

    for (int i = 0; i < rm->slave_count; i++) {
        replication_connection *slave = &rm->slave_conns[i];
        if (slave->running) {
            slave->running = false;
            if (slave->sockfd > 0) {
                close(slave->sockfd);
                slave->sockfd = -1;
            }
        }
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Replication manager stopped successfully");
    return SUCCESS;
}

int replication_manager_add_slave(replication_manager *rm, const char *host, int port, const char *user, const char *password) {
    if (!rm || !host) {
        return ERROR_INVALID_PARAMETER;
    }

    if (rm->slave_count >= MAX_REPLICAS) {
        return ERROR_LIMIT_EXCEEDED;
    }

    replication_connection *slave = &rm->slave_conns[rm->slave_count++];
    slave->host = strdup(host);
    slave->port = port;
    slave->user = strdup(user);
    slave->password = strdup(password);
    slave->role = REPLICATION_ROLE_SLAVE;
    slave->state = REPLICATION_STATE_INIT;
    slave->running = false;

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Added slave %s:%d", host, port);
    return SUCCESS;
}

int replication_manager_remove_slave(replication_manager *rm, const char *host, int port) {
    if (!rm || !host) {
        return ERROR_INVALID_PARAMETER;
    }

    for (int i = 0; i < rm->slave_count; i++) {
        replication_connection *slave = &rm->slave_conns[i];
        if (strcmp(slave->host, host) == 0 && slave->port == port) {
            if (slave->running) {
                slave->running = false;
                if (slave->sockfd > 0) {
                    close(slave->sockfd);
                    slave->sockfd = -1;
                }
            }
            if (slave->host) free(slave->host);
            if (slave->user) free(slave->user);
            if (slave->password) free(slave->password);
            if (slave->binlog_filename) free(slave->binlog_filename);
            memmove(&rm->slave_conns[i], &rm->slave_conns[i + 1], (rm->slave_count - i - 1) * sizeof(replication_connection));
            rm->slave_count--;
            logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Removed slave %s:%d", host, port);
            return SUCCESS;
        }
    }

    return ERROR_NOT_FOUND;
}

int replication_manager_connect_to_master(replication_manager *rm, const char *host, int port, const char *user, const char *password) {
    if (!rm || !host || !user || !password) {
        return ERROR_INVALID_PARAMETER;
    }

    if (!rm->master_conn) {
        rm->master_conn = (replication_connection *)malloc(sizeof(replication_connection));
        if (!rm->master_conn) {
            return ERROR_OUT_OF_MEMORY;
        }
        memset(rm->master_conn, 0, sizeof(replication_connection));
    }

    replication_connection *master = rm->master_conn;
    master->host = strdup(host);
    master->port = port;
    master->user = strdup(user);
    master->password = strdup(password);
    master->role = REPLICATION_ROLE_MASTER;
    master->state = REPLICATION_STATE_CONNECTING;

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Connecting to master %s:%d", host, port);

    master->state = REPLICATION_STATE_RUNNING;
    return SUCCESS;
}

int replication_manager_disconnect_from_master(replication_manager *rm) {
    if (!rm || !rm->master_conn) {
        return SUCCESS;
    }

    replication_connection *master = rm->master_conn;
    if (master->running) {
        master->running = false;
        if (master->sockfd > 0) {
            close(master->sockfd);
            master->sockfd = -1;
        }
    }

    master->state = REPLICATION_STATE_STOPPED;
    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Disconnected from master %s:%d", master->host, master->port);
    return SUCCESS;
}

int replication_manager_write_binlog(replication_manager *rm, int event_type, const char *data, int data_length) {
    if (!rm || !data) {
        return ERROR_INVALID_PARAMETER;
    }

    if (!rm->running || rm->state != REPLICATION_STATE_RUNNING) {
        return ERROR_OPERATION_FAILED;
    }

    binlog_event event;
    event.timestamp = (uint64_t)time(NULL);
    event.event_id = rm->binlog_manager->next_event_id++;
    event.event_type = event_type;
    event.data_length = data_length;
    event.data = (char *)data;
    event.gtid = event.event_id;

    int ret = binlog_manager_write(rm->binlog_manager, &event);
    if (ret != SUCCESS) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_REPLICATION, ret, "Failed to write binlog event");
        return ret;
    }

    return SUCCESS;
}

int replication_manager_read_binlog(replication_manager *rm, const char *filename, uint64_t position, binlog_event *event) {
    if (!rm || !filename || !event) {
        return ERROR_INVALID_PARAMETER;
    }

    return SUCCESS;
}

int replication_manager_sync_with_master(replication_manager *rm) {
    if (!rm) {
        return ERROR_INVALID_PARAMETER;
    }

    if (rm->role != REPLICATION_ROLE_SLAVE && rm->role != REPLICATION_ROLE_BOTH) {
        return ERROR_OPERATION_FAILED;
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Syncing with master");
    return SUCCESS;
}

int replication_manager_sync_with_slave(replication_manager *rm, replication_connection *slave) {
    if (!rm || !slave) {
        return ERROR_INVALID_PARAMETER;
    }

    if (rm->role != REPLICATION_ROLE_MASTER && rm->role != REPLICATION_ROLE_BOTH) {
        return ERROR_OPERATION_FAILED;
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Syncing with slave %s:%d", slave->host, slave->port);
    return SUCCESS;
}

int replication_manager_get_role(replication_manager *rm) {
    if (!rm) {
        return ERROR_INVALID_PARAMETER;
    }
    return rm->role;
}

int replication_manager_get_state(replication_manager *rm) {
    if (!rm) {
        return ERROR_INVALID_PARAMETER;
    }
    return rm->state;
}

int replication_manager_get_slave_count(replication_manager *rm) {
    if (!rm) {
        return ERROR_INVALID_PARAMETER;
    }
    return rm->slave_count;
}

void replication_manager_print_status(replication_manager *rm) {
    if (!rm) {
        return;
    }

    printf("Replication Status:\n");
    printf("Server ID: %s\n", rm->server_id);
    printf("Role: %s\n", rm->role == REPLICATION_ROLE_MASTER ? "Master" : rm->role == REPLICATION_ROLE_SLAVE ? "Slave" : "Both");
    printf("State: %s\n", rm->state == REPLICATION_STATE_INIT ? "Init" : 
           rm->state == REPLICATION_STATE_CONNECTING ? "Connecting" : 
           rm->state == REPLICATION_STATE_SYNCING ? "Syncing" : 
           rm->state == REPLICATION_STATE_RUNNING ? "Running" : 
           rm->state == REPLICATION_STATE_ERROR ? "Error" : "Stopped");
    printf("Read Only: %s\n", rm->read_only ? "Yes" : "No");
    printf("Slave Count: %d\n", rm->slave_count);

    if (rm->role == REPLICATION_ROLE_SLAVE || rm->role == REPLICATION_ROLE_BOTH) {
        printf("Master: %s:%d\n", rm->master_host, rm->master_port);
    }

    for (int i = 0; i < rm->slave_count; i++) {
        replication_connection *slave = &rm->slave_conns[i];
        printf("Slave %d: %s:%d, State: %s\n", i + 1, slave->host, slave->port, 
               slave->state == REPLICATION_STATE_INIT ? "Init" : 
               slave->state == REPLICATION_STATE_CONNECTING ? "Connecting" : 
               slave->state == REPLICATION_STATE_SYNCING ? "Syncing" : 
               slave->state == REPLICATION_STATE_RUNNING ? "Running" : 
               slave->state == REPLICATION_STATE_ERROR ? "Error" : "Stopped");
    }
}

static binlog_manager *binlog_manager_create(config_t *config) {
    binlog_manager *bm = (binlog_manager *)malloc(sizeof(binlog_manager));
    if (!bm) {
        return NULL;
    }

    memset(bm, 0, sizeof(binlog_manager));
    bm->binlog_dir = strdup(config_get_string(config, "replication", "binlog_dir", "./binlog"));
    bm->max_binlog_size = config_get_int(config, "replication", "max_binlog_size", MAX_BINLOG_SIZE);
    bm->max_binlog_files = config_get_int(config, "replication", "max_binlog_files", MAX_BINLOG_FILES);
    bm->enabled = config_get_bool(config, "replication", "binlog_enabled", true);
    bm->sync_binlog = config_get_bool(config, "replication", "sync_binlog", false);
    bm->flush_interval = config_get_int(config, "replication", "binlog_flush_interval", BINLOG_FLUSH_INTERVAL);
    bm->next_event_id = 1;
    bm->binlog_count = 0;

    struct stat st;
    if (stat(bm->binlog_dir, &st) != 0) {
        mkdir(bm->binlog_dir, 0755);
    }

    return bm;
}

static void binlog_manager_destroy(binlog_manager *bm) {
    if (bm) {
        if (bm->current_binlog) {
            if (bm->current_binlog->file) {
                fclose(bm->current_binlog->file);
            }
            if (bm->current_binlog->filename) {
                free(bm->current_binlog->filename);
            }
            free(bm->current_binlog);
        }
        if (bm->binlogs) {
            for (int i = 0; i < bm->binlog_count; i++) {
                if (bm->binlogs[i].filename) {
                    free(bm->binlogs[i].filename);
                }
            }
            free(bm->binlogs);
        }
        if (bm->binlog_dir) {
            free(bm->binlog_dir);
        }
        free(bm);
    }
}

static int binlog_manager_open(binlog_manager *bm) {
    if (!bm || !bm->enabled) {
        return SUCCESS;
    }

    char filename[256];
    snprintf(filename, sizeof(filename), "%s/binlog.%llu", bm->binlog_dir, (unsigned long long)time(NULL));

    bm->current_binlog = (binlog_file *)malloc(sizeof(binlog_file));
    if (!bm->current_binlog) {
        return ERROR_OUT_OF_MEMORY;
    }

    memset(bm->current_binlog, 0, sizeof(binlog_file));
    bm->current_binlog->filename = strdup(filename);
    bm->current_binlog->file = fopen(filename, "a+");
    if (!bm->current_binlog->file) {
        free(bm->current_binlog->filename);
        free(bm->current_binlog);
        bm->current_binlog = NULL;
        return ERROR_OPERATION_FAILED;
    }

    fseek(bm->current_binlog->file, 0, SEEK_END);
    bm->current_binlog->file_size = ftell(bm->current_binlog->file);
    bm->current_binlog->start_pos = 0;
    bm->current_binlog->end_pos = bm->current_binlog->file_size;
    bm->current_binlog->first_event_id = bm->next_event_id;

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Opened binlog file %s", filename);
    return SUCCESS;
}

static int binlog_manager_write(binlog_manager *bm, binlog_event *event) {
    if (!bm || !bm->current_binlog || !bm->current_binlog->file || !event) {
        return ERROR_INVALID_PARAMETER;
    }

    if (bm->current_binlog->file_size >= bm->max_binlog_size) {
        int ret = binlog_manager_rotate(bm);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    size_t written = fwrite(event, sizeof(binlog_event), 1, bm->current_binlog->file);
    if (written != 1) {
        return ERROR_OPERATION_FAILED;
    }

    if (bm->sync_binlog) {
        fflush(bm->current_binlog->file);
    }

    bm->current_binlog->file_size += sizeof(binlog_event);
    bm->current_binlog->end_pos = bm->current_binlog->file_size;
    bm->current_binlog->last_event_id = event->event_id;

    return SUCCESS;
}

static int binlog_manager_rotate(binlog_manager *bm) {
    if (!bm || !bm->current_binlog) {
        return ERROR_INVALID_PARAMETER;
    }

    if (bm->current_binlog->file) {
        fclose(bm->current_binlog->file);
        bm->current_binlog->file = NULL;
    }

    bm->binlogs = (binlog_file *)realloc(bm->binlogs, (bm->binlog_count + 1) * sizeof(binlog_file));
    if (!bm->binlogs) {
        return ERROR_OUT_OF_MEMORY;
    }

    bm->binlogs[bm->binlog_count++] = *bm->current_binlog;

    char filename[256];
    snprintf(filename, sizeof(filename), "%s/binlog.%llu", bm->binlog_dir, (unsigned long long)time(NULL));

    bm->current_binlog->filename = strdup(filename);
    bm->current_binlog->file = fopen(filename, "a+");
    if (!bm->current_binlog->file) {
        free(bm->current_binlog->filename);
        return ERROR_OPERATION_FAILED;
    }

    bm->current_binlog->file_size = 0;
    bm->current_binlog->start_pos = 0;
    bm->current_binlog->end_pos = 0;
    bm->current_binlog->first_event_id = bm->next_event_id;
    bm->current_binlog->last_event_id = 0;

    if (bm->binlog_count > bm->max_binlog_files) {
        binlog_file *oldest = &bm->binlogs[0];
        remove(oldest->filename);
        free(oldest->filename);
        memmove(&bm->binlogs[0], &bm->binlogs[1], (bm->binlog_count - 1) * sizeof(binlog_file));
        bm->binlog_count--;
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_REPLICATION, "Rotated binlog to %s", filename);
    return SUCCESS;
}

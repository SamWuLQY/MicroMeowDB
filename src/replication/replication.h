#ifndef REPLICATION_H
#define REPLICATION_H

#include "../config/config.h"
#include "../error/error.h"
#include "../logging/logging.h"
#include "../network/network.h"

#define REPLICATION_MODULE "replication"

#define MAX_REPLICAS 32
#define MAX_REPLICATION_THREADS 8
#define MAX_BINLOG_SIZE 1024 * 1024 * 1024
#define MAX_BINLOG_FILES 100
#define BINLOG_FLUSH_INTERVAL 1000

#define REPLICATION_ROLE_MASTER 1
#define REPLICATION_ROLE_SLAVE 2
#define REPLICATION_ROLE_BOTH 3

#define REPLICATION_STATE_INIT 1
#define REPLICATION_STATE_CONNECTING 2
#define REPLICATION_STATE_SYNCING 3
#define REPLICATION_STATE_RUNNING 4
#define REPLICATION_STATE_ERROR 5
#define REPLICATION_STATE_STOPPED 6

#define BINLOG_EVENT_TYPE_QUERY 1
#define BINLOG_EVENT_TYPE_WRITE 2
#define BINLOG_EVENT_TYPE_UPDATE 3
#define BINLOG_EVENT_TYPE_DELETE 4
#define BINLOG_EVENT_TYPE_ROLLBACK 5
#define BINLOG_EVENT_TYPE_COMMIT 6
#define BINLOG_EVENT_TYPE_BEGIN 7
#define BINLOG_EVENT_TYPE_GTID 8
#define BINLOG_EVENT_TYPE_FORMAT_DESCRIPTION 9
#define BINLOG_EVENT_TYPE_ROTATE 10

#define SYNC_TYPE_FULL 1
#define SYNC_TYPE_INCREMENTAL 2

// 二进制日志事件结构
typedef struct {
    uint64_t timestamp;
    uint64_t event_id;
    uint32_t event_type;
    uint32_t data_length;
    char *data;
    uint64_t gtid;
} binlog_event;

// 二进制日志文件结构
typedef struct {
    char *filename;
    uint64_t file_size;
    uint64_t start_pos;
    uint64_t end_pos;
    uint64_t first_event_id;
    uint64_t last_event_id;
    FILE *file;
} binlog_file;

// 二进制日志管理器结构
typedef struct {
    char *binlog_dir;
    binlog_file *current_binlog;
    binlog_file *binlogs;
    int binlog_count;
    uint64_t next_event_id;
    uint64_t max_binlog_size;
    int max_binlog_files;
    bool enabled;
    bool sync_binlog;
    uint32_t flush_interval;
} binlog_manager;

// 复制连接结构
typedef struct {
    int sockfd;
    char *host;
    int port;
    char *user;
    char *password;
    int role;
    int state;
    uint64_t last_event_id;
    uint64_t last_gtid;
    uint64_t last_timestamp;
    char *binlog_filename;
    uint64_t binlog_position;
    pthread_t thread_id;
    bool running;
} replication_connection;

// 复制管理器结构
typedef struct {
    config_t *config;
    network_server *server;
    int role;
    int state;
    char *server_id;
    char *master_host;
    int master_port;
    char *master_user;
    char *master_password;
    char *replicate_do_db;
    char *replicate_ignore_db;
    char *replicate_do_table;
    char *replicate_ignore_table;
    bool read_only;
    int sync_type;
    binlog_manager *binlog_manager;
    replication_connection *master_conn;
    replication_connection *slave_conns;
    int slave_count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool initialized;
    bool running;
} replication_manager;

// 复制配置结构
typedef struct {
    char *server_id;
    int role;
    char *binlog_dir;
    bool binlog_enabled;
    uint64_t max_binlog_size;
    int max_binlog_files;
    bool sync_binlog;
    uint32_t binlog_flush_interval;
    char *master_host;
    int master_port;
    char *master_user;
    char *master_password;
    char *replicate_do_db;
    char *replicate_ignore_db;
    char *replicate_do_table;
    char *replicate_ignore_table;
    bool read_only;
    int sync_type;
    int slave_threads;
} replication_config;

replication_manager *replication_manager_create(config_t *config, network_server *server);
void replication_manager_destroy(replication_manager *rm);

int replication_manager_start(replication_manager *rm);
int replication_manager_stop(replication_manager *rm);

int replication_manager_add_slave(replication_manager *rm, const char *host, int port, const char *user, const char *password);
int replication_manager_remove_slave(replication_manager *rm, const char *host, int port);

int replication_manager_connect_to_master(replication_manager *rm, const char *host, int port, const char *user, const char *password);
int replication_manager_disconnect_from_master(replication_manager *rm);

int replication_manager_write_binlog(replication_manager *rm, int event_type, const char *data, int data_length);
int replication_manager_read_binlog(replication_manager *rm, const char *filename, uint64_t position, binlog_event *event);

int replication_manager_sync_with_master(replication_manager *rm);
int replication_manager_sync_with_slave(replication_manager *rm, replication_connection *slave);

int replication_manager_get_role(replication_manager *rm);
int replication_manager_get_state(replication_manager *rm);
int replication_manager_get_slave_count(replication_manager *rm);

void replication_manager_print_status(replication_manager *rm);

#endif
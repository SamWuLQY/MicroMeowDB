#ifndef CLIENT_H
#define CLIENT_H

#include "../config/config.h"
#include "../error/error.h"
#include "../logging/logging.h"
#include "../network/network.h"

#define CLIENT_MODULE "client"

#define MAX_COMMAND_LENGTH 10240
#define MAX_COMMAND_HISTORY 1000
#define MAX_CONNECTIONS 10
#define MAX_RESULT_ROWS 10000
#define MAX_RESULT_COLS 100

#define PROMPT "MicroMeowDB> "
#define CONTINUE_PROMPT "> "

#define COMMAND_TYPE_QUERY 1
#define COMMAND_TYPE_HELP 2
#define COMMAND_TYPE_CONNECT 3
#define COMMAND_TYPE_DISCONNECT 4
#define COMMAND_TYPE_EXIT 5
#define COMMAND_TYPE_STATUS 6
#define COMMAND_TYPE_SET 7
#define COMMAND_TYPE_SHOW 8
#define COMMAND_TYPE_USE 9
#define COMMAND_TYPE_SOURCE 10
#define COMMAND_TYPE_QUIT 11
#define COMMAND_TYPE_EXPLAIN 12
#define COMMAND_TYPE_BACKUP 13
#define COMMAND_TYPE_RESTORE 14

// 命令结构
typedef struct {
    int type;
    char *text;
    char *args;
} command;

// 连接结构
typedef struct {
    int sockfd;
    char *host;
    int port;
    char *user;
    char *password;
    char *database;
    bool connected;
    uint64_t connected_at;
} connection;

// 结果集结构
typedef struct {
    char **columns;
    int column_count;
    char ***rows;
    int row_count;
    int affected_rows;
    int last_insert_id;
    char *message;
} result_set;

// 客户端结构
typedef struct {
    config_t *config;
    connection *connections;
    int connection_count;
    int current_connection;
    command *command_history;
    int command_history_count;
    char *prompt;
    bool interactive;
    bool quiet;
    bool batch;
    char *batch_file;
    FILE *batch_fp;
    result_set *last_result;
    bool initialized;
} client;

// 客户端配置结构
typedef struct {
    char *host;
    int port;
    char *user;
    char *password;
    char *database;
    char *default_character_set;
    bool interactive;
    bool quiet;
    bool batch;
    char *batch_file;
    char *prompt;
    int command_history_size;
} client_config;

client *client_create(const client_config *config);
void client_destroy(client *cli);

int client_connect(client *cli, const char *host, int port, const char *user, const char *password, const char *database);
int client_disconnect(client *cli, int conn_id);
int client_execute(client *cli, const char *command, result_set **result);
int client_process_command(client *cli, const char *command_text);

int client_start_interactive(client *cli);
int client_run_batch(client *cli, const char *file);

void client_print_result(client *cli, result_set *result);
void client_print_error(client *cli, int error_code, const char *message);
void client_print_prompt(client *cli);

void client_add_to_history(client *cli, const char *command);
char *client_get_from_history(client *cli, int index);

result_set *result_set_create(void);
void result_set_destroy(result_set *result);

#endif
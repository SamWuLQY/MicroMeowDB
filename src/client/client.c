#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <readline/readline.h>
#include <readline/history.h>

static int parse_command(client *cli, const char *command_text, command *cmd);
static int execute_query(client *cli, const char *query, result_set **result);
static int execute_help(client *cli, const char *args);
static int execute_status(client *cli);
static int execute_set(client *cli, const char *args);
static int execute_show(client *cli, const char *args);
static int execute_use(client *cli, const char *database);
static int execute_source(client *cli, const char *file);
static int execute_explain(client *cli, const char *query);
static int execute_backup(client *cli, const char *args);
static int execute_restore(client *cli, const char *args);

client *client_create(const client_config *config) {
    client *cli = (client *)malloc(sizeof(client));
    if (!cli) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_CLIENT, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for client");
        return NULL;
    }

    memset(cli, 0, sizeof(client));
    cli->config = config ? (config_t *)config : config_init(NULL);
    cli->connections = (connection *)malloc(MAX_CONNECTIONS * sizeof(connection));
    if (!cli->connections) {
        free(cli);
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_CLIENT, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for connections");
        return NULL;
    }

    cli->command_history = (command *)malloc(MAX_COMMAND_HISTORY * sizeof(command));
    if (!cli->command_history) {
        free(cli->connections);
        free(cli);
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_CLIENT, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for command history");
        return NULL;
    }

    cli->prompt = strdup(config ? config->prompt : PROMPT);
    cli->interactive = config ? config->interactive : isatty(fileno(stdin));
    cli->quiet = config ? config->quiet : false;
    cli->batch = config ? config->batch : false;
    if (config && config->batch_file) {
        cli->batch_file = strdup(config->batch_file);
    }

    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        cli->connections[i].sockfd = -1;
        cli->connections[i].connected = false;
    }

    cli->connection_count = 0;
    cli->current_connection = -1;
    cli->command_history_count = 0;
    cli->last_result = NULL;
    cli->initialized = true;

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_CLIENT, "Client created successfully");
    return cli;
}

void client_destroy(client *cli) {
    if (cli) {
        for (int i = 0; i < cli->connection_count; i++) {
            if (cli->connections[i].connected) {
                client_disconnect(cli, i);
            }
            if (cli->connections[i].host) free(cli->connections[i].host);
            if (cli->connections[i].user) free(cli->connections[i].user);
            if (cli->connections[i].password) free(cli->connections[i].password);
            if (cli->connections[i].database) free(cli->connections[i].database);
        }
        if (cli->connections) {
            free(cli->connections);
        }
        for (int i = 0; i < cli->command_history_count; i++) {
            if (cli->command_history[i].text) free(cli->command_history[i].text);
            if (cli->command_history[i].args) free(cli->command_history[i].args);
        }
        if (cli->command_history) {
            free(cli->command_history);
        }
        if (cli->prompt) free(cli->prompt);
        if (cli->batch_file) free(cli->batch_file);
        if (cli->batch_fp) fclose(cli->batch_fp);
        if (cli->last_result) {
            result_set_destroy(cli->last_result);
        }
        if (cli->config) {
            config_destroy(cli->config);
        }
        free(cli);
    }
}

int client_connect(client *cli, const char *host, int port, const char *user, const char *password, const char *database) {
    if (!cli || !host || !user) {
        return ERROR_INVALID_PARAMETER;
    }

    if (cli->connection_count >= MAX_CONNECTIONS) {
        return ERROR_LIMIT_EXCEEDED;
    }

    connection *conn = &cli->connections[cli->connection_count++];
    conn->host = strdup(host);
    conn->port = port;
    conn->user = strdup(user);
    conn->password = strdup(password);
    if (database) {
        conn->database = strdup(database);
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_CLIENT, "Connecting to %s:%d as %s", host, port, user);

    conn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->sockfd < 0) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_CLIENT, ERROR_OPERATION_FAILED, "Failed to create socket");
        return ERROR_OPERATION_FAILED;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(host);

    if (connect(conn->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_CLIENT, ERROR_OPERATION_FAILED, "Failed to connect to server");
        close(conn->sockfd);
        conn->sockfd = -1;
        return ERROR_OPERATION_FAILED;
    }

    conn->connected = true;
    conn->connected_at = (uint64_t)time(NULL);
    cli->current_connection = cli->connection_count - 1;

    printf("Connected to MicroMeowDB server at %s:%d\n", host, port);
    return SUCCESS;
}

int client_disconnect(client *cli, int conn_id) {
    if (!cli || conn_id < 0 || conn_id >= cli->connection_count) {
        return ERROR_INVALID_PARAMETER;
    }

    connection *conn = &cli->connections[conn_id];
    if (!conn->connected) {
        return SUCCESS;
    }

    close(conn->sockfd);
    conn->sockfd = -1;
    conn->connected = false;

    printf("Disconnected from %s:%d\n", conn->host, conn->port);
    return SUCCESS;
}

int client_execute(client *cli, const char *command, result_set **result) {
    if (!cli || !command) {
        return ERROR_INVALID_PARAMETER;
    }

    if (cli->current_connection < 0 || !cli->connections[cli->current_connection].connected) {
        printf("Not connected to any server. Use CONNECT command to connect.\n");
        return ERROR_OPERATION_FAILED;
    }

    result_set *rs = result_set_create();
    if (!rs) {
        return ERROR_OUT_OF_MEMORY;
    }

    int ret = execute_query(cli, command, &rs);
    if (ret != SUCCESS) {
        result_set_destroy(rs);
        return ret;
    }

    if (result) {
        *result = rs;
    } else {
        client_print_result(cli, rs);
        result_set_destroy(rs);
    }

    return SUCCESS;
}

int client_process_command(client *cli, const char *command_text) {
    if (!cli || !command_text) {
        return ERROR_INVALID_PARAMETER;
    }

    command cmd;
    int ret = parse_command(cli, command_text, &cmd);
    if (ret != SUCCESS) {
        return ret;
    }

    client_add_to_history(cli, command_text);

    switch (cmd.type) {
        case COMMAND_TYPE_QUERY:
            {
                result_set *result;
                ret = execute_query(cli, cmd.text, &result);
                if (ret == SUCCESS) {
                    client_print_result(cli, result);
                    result_set_destroy(result);
                }
            }
            break;
        case COMMAND_TYPE_HELP:
            ret = execute_help(cli, cmd.args);
            break;
        case COMMAND_TYPE_CONNECT:
            {
                char host[256], user[256], password[256], database[256];
                int port = 3306;
                sscanf(cmd.args, "%s %d %s %s %s", host, &port, user, password, database);
                ret = client_connect(cli, host, port, user, password, database);
            }
            break;
        case COMMAND_TYPE_DISCONNECT:
            ret = client_disconnect(cli, cli->current_connection);
            break;
        case COMMAND_TYPE_EXIT:
        case COMMAND_TYPE_QUIT:
            return 0;
        case COMMAND_TYPE_STATUS:
            ret = execute_status(cli);
            break;
        case COMMAND_TYPE_SET:
            ret = execute_set(cli, cmd.args);
            break;
        case COMMAND_TYPE_SHOW:
            ret = execute_show(cli, cmd.args);
            break;
        case COMMAND_TYPE_USE:
            ret = execute_use(cli, cmd.args);
            break;
        case COMMAND_TYPE_SOURCE:
            ret = execute_source(cli, cmd.args);
            break;
        case COMMAND_TYPE_EXPLAIN:
            ret = execute_explain(cli, cmd.args);
            break;
        case COMMAND_TYPE_BACKUP:
            ret = execute_backup(cli, cmd.args);
            break;
        case COMMAND_TYPE_RESTORE:
            ret = execute_restore(cli, cmd.args);
            break;
        default:
            ret = ERROR_INVALID_PARAMETER;
            break;
    }

    if (cmd.text) free(cmd.text);
    if (cmd.args) free(cmd.args);

    return ret;
}

int client_start_interactive(client *cli) {
    if (!cli) {
        return ERROR_INVALID_PARAMETER;
    }

    char *line, *prompt;
    cli->interactive = true;

    printf("Welcome to MicroMeowDB client\n");
    printf("Type 'HELP' for help, 'EXIT' to exit.\n\n");

    while (1) {
        prompt = cli->prompt;
        line = readline(prompt);
        if (!line) {
            break;
        }

        if (strlen(line) > 0) {
            add_history(line);
            int ret = client_process_command(cli, line);
            if (ret == 0) {
                free(line);
                break;
            }
        }

        free(line);
    }

    printf("\nGoodbye!\n");
    return SUCCESS;
}

int client_run_batch(client *cli, const char *file) {
    if (!cli || !file) {
        return ERROR_INVALID_PARAMETER;
    }

    FILE *fp = fopen(file, "r");
    if (!fp) {
        printf("Failed to open file: %s\n", file);
        return ERROR_OPERATION_FAILED;
    }

    char line[MAX_COMMAND_LENGTH];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0 && line[0] != '#') {
            int ret = client_process_command(cli, line);
            if (ret == 0) {
                break;
            }
        }
    }

    fclose(fp);
    return SUCCESS;
}

void client_print_result(client *cli, result_set *result) {
    if (!cli || !result) {
        return;
    }

    if (result->message) {
        printf("%s\n", result->message);
    }

    if (result->affected_rows > 0) {
        printf("%d rows affected\n", result->affected_rows);
    }

    if (result->last_insert_id > 0) {
        printf("Last insert ID: %d\n", result->last_insert_id);
    }

    if (result->row_count > 0) {
        for (int i = 0; i < result->column_count; i++) {
            printf("%-20s ", result->columns[i]);
        }
        printf("\n");

        for (int i = 0; i < result->column_count; i++) {
            printf("==================== ");
        }
        printf("\n");

        for (int i = 0; i < result->row_count; i++) {
            for (int j = 0; j < result->column_count; j++) {
                printf("%-20s ", result->rows[i][j]);
            }
            printf("\n");
        }

        printf("\n%d rows in set\n", result->row_count);
    }
}

void client_print_error(client *cli, int error_code, const char *message) {
    if (!cli) {
        return;
    }

    printf("Error %d: %s\n", error_code, message);
}

void client_print_prompt(client *cli) {
    if (!cli || !cli->interactive) {
        return;
    }

    printf("%s", cli->prompt);
    fflush(stdout);
}

void client_add_to_history(client *cli, const char *command) {
    if (!cli || !command) {
        return;
    }

    if (cli->command_history_count >= MAX_COMMAND_HISTORY) {
        if (cli->command_history[0].text) free(cli->command_history[0].text);
        if (cli->command_history[0].args) free(cli->command_history[0].args);
        memmove(&cli->command_history[0], &cli->command_history[1], (MAX_COMMAND_HISTORY - 1) * sizeof(command));
        cli->command_history_count--;
    }

    command *cmd = &cli->command_history[cli->command_history_count++];
    cmd->text = strdup(command);
}

char *client_get_from_history(client *cli, int index) {
    if (!cli || index < 0 || index >= cli->command_history_count) {
        return NULL;
    }

    return cli->command_history[index].text;
}

result_set *result_set_create(void) {
    result_set *rs = (result_set *)malloc(sizeof(result_set));
    if (!rs) {
        return NULL;
    }

    memset(rs, 0, sizeof(result_set));
    return rs;
}

void result_set_destroy(result_set *result) {
    if (result) {
        if (result->columns) {
            for (int i = 0; i < result->column_count; i++) {
                free(result->columns[i]);
            }
            free(result->columns);
        }
        if (result->rows) {
            for (int i = 0; i < result->row_count; i++) {
                for (int j = 0; j < result->column_count; j++) {
                    free(result->rows[i][j]);
                }
                free(result->rows[i]);
            }
            free(result->rows);
        }
        if (result->message) {
            free(result->message);
        }
        free(result);
    }
}

static int parse_command(client *cli, const char *command_text, command *cmd) {
    if (!cli || !command_text || !cmd) {
        return ERROR_INVALID_PARAMETER;
    }

    char *command_copy = strdup(command_text);
    if (!command_copy) {
        return ERROR_OUT_OF_MEMORY;
    }

    char *token = strtok(command_copy, " ");
    if (!token) {
        free(command_copy);
        return ERROR_INVALID_PARAMETER;
    }

    if (strcasecmp(token, "HELP") == 0) {
        cmd->type = COMMAND_TYPE_HELP;
        cmd->args = strdup(strtok(NULL, ""));
    } else if (strcasecmp(token, "CONNECT") == 0) {
        cmd->type = COMMAND_TYPE_CONNECT;
        cmd->args = strdup(strtok(NULL, ""));
    } else if (strcasecmp(token, "DISCONNECT") == 0) {
        cmd->type = COMMAND_TYPE_DISCONNECT;
    } else if (strcasecmp(token, "EXIT") == 0) {
        cmd->type = COMMAND_TYPE_EXIT;
    } else if (strcasecmp(token, "STATUS") == 0) {
        cmd->type = COMMAND_TYPE_STATUS;
    } else if (strcasecmp(token, "SET") == 0) {
        cmd->type = COMMAND_TYPE_SET;
        cmd->args = strdup(strtok(NULL, ""));
    } else if (strcasecmp(token, "SHOW") == 0) {
        cmd->type = COMMAND_TYPE_SHOW;
        cmd->args = strdup(strtok(NULL, ""));
    } else if (strcasecmp(token, "USE") == 0) {
        cmd->type = COMMAND_TYPE_USE;
        cmd->args = strdup(strtok(NULL, ""));
    } else if (strcasecmp(token, "SOURCE") == 0) {
        cmd->type = COMMAND_TYPE_SOURCE;
        cmd->args = strdup(strtok(NULL, ""));
    } else if (strcasecmp(token, "QUIT") == 0) {
        cmd->type = COMMAND_TYPE_QUIT;
    } else if (strcasecmp(token, "EXPLAIN") == 0) {
        cmd->type = COMMAND_TYPE_EXPLAIN;
        cmd->args = strdup(strtok(NULL, ""));
    } else if (strcasecmp(token, "BACKUP") == 0) {
        cmd->type = COMMAND_TYPE_BACKUP;
        cmd->args = strdup(strtok(NULL, ""));
    } else if (strcasecmp(token, "RESTORE") == 0) {
        cmd->type = COMMAND_TYPE_RESTORE;
        cmd->args = strdup(strtok(NULL, ""));
    } else {
        cmd->type = COMMAND_TYPE_QUERY;
        cmd->text = strdup(command_text);
    }

    free(command_copy);
    return SUCCESS;
}

static int execute_query(client *cli, const char *query, result_set **result) {
    if (!cli || !query) {
        return ERROR_INVALID_PARAMETER;
    }

    result_set *rs = result_set_create();
    if (!rs) {
        return ERROR_OUT_OF_MEMORY;
    }

    rs->message = strdup("Query executed successfully");
    rs->affected_rows = 0;
    rs->last_insert_id = 0;

    if (result) {
        *result = rs;
    }

    return SUCCESS;
}

static int execute_help(client *cli, const char *args) {
    printf("MicroMeowDB Client Help\n");
    printf("====================\n");
    printf("HELP [command]      - Show help information\n");
    printf("CONNECT host port user password [database] - Connect to server\n");
    printf("DISCONNECT         - Disconnect from current server\n");
    printf("EXIT/QUIT          - Exit the client\n");
    printf("STATUS             - Show connection status\n");
    printf("SET variable value - Set client variable\n");
    printf("SHOW [variable]    - Show client variables\n");
    printf("USE database       - Use specified database\n");
    printf("SOURCE file        - Execute commands from file\n");
    printf("EXPLAIN query      - Explain query execution plan\n");
    printf("BACKUP [options]   - Backup database\n");
    printf("RESTORE [options]  - Restore database\n");
    printf("\nType any SQL query to execute it\n");
    return SUCCESS;
}

static int execute_status(client *cli) {
    if (!cli) {
        return ERROR_INVALID_PARAMETER;
    }

    printf("Client Status\n");
    printf("============\n");
    printf("Interactive: %s\n", cli->interactive ? "Yes" : "No");
    printf("Quiet: %s\n", cli->quiet ? "Yes" : "No");
    printf("Batch: %s\n", cli->batch ? "Yes" : "No");
    printf("Command History: %d\n", cli->command_history_count);
    printf("Connections: %d\n", cli->connection_count);

    for (int i = 0; i < cli->connection_count; i++) {
        connection *conn = &cli->connections[i];
        printf("Connection %d: %s:%d - %s\n", i + 1, conn->host, conn->port, conn->connected ? "Connected" : "Disconnected");
    }

    return SUCCESS;
}

static int execute_set(client *cli, const char *args) {
    if (!cli || !args) {
        return ERROR_INVALID_PARAMETER;
    }

    printf("SET command not implemented yet\n");
    return SUCCESS;
}

static int execute_show(client *cli, const char *args) {
    if (!cli) {
        return ERROR_INVALID_PARAMETER;
    }

    printf("SHOW command not implemented yet\n");
    return SUCCESS;
}

static int execute_use(client *cli, const char *database) {
    if (!cli || !database) {
        return ERROR_INVALID_PARAMETER;
    }

    if (cli->current_connection >= 0) {
        connection *conn = &cli->connections[cli->current_connection];
        if (conn->database) {
            free(conn->database);
        }
        conn->database = strdup(database);
        printf("Using database %s\n", database);
    }

    return SUCCESS;
}

static int execute_source(client *cli, const char *file) {
    if (!cli || !file) {
        return ERROR_INVALID_PARAMETER;
    }

    return client_run_batch(cli, file);
}

static int execute_explain(client *cli, const char *query) {
    if (!cli || !query) {
        return ERROR_INVALID_PARAMETER;
    }

    printf("EXPLAIN command not implemented yet\n");
    return SUCCESS;
}

static int execute_backup(client *cli, const char *args) {
    if (!cli) {
        return ERROR_INVALID_PARAMETER;
    }

    printf("BACKUP command not implemented yet\n");
    return SUCCESS;
}

static int execute_restore(client *cli, const char *args) {
    if (!cli) {
        return ERROR_INVALID_PARAMETER;
    }

    printf("RESTORE command not implemented yet\n");
    return SUCCESS;
}

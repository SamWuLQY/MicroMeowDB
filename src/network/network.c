#include "network.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 全局连接ID计数器
static uint32_t global_connection_id = 0;

// 初始化网络库
network_error network_init(void) {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return NETWORK_ERROR_INIT;
    }
#endif
    return NETWORK_SUCCESS;
}

// 清理网络库
void network_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

// 创建套接字
static socket_t create_socket(void) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET_VALUE) {
        return INVALID_SOCKET_VALUE;
    }
    return sock;
}

// 设置套接字选项
static network_error set_socket_options(socket_t socket, uint32_t send_buffer, uint32_t recv_buffer) {
    int optval = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval)) == SOCKET_ERROR_VALUE) {
        return NETWORK_ERROR_SOCKET;
    }
    
    if (send_buffer > 0) {
        if (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (const char *)&send_buffer, sizeof(send_buffer)) == SOCKET_ERROR_VALUE) {
            return NETWORK_ERROR_SOCKET;
        }
    }
    
    if (recv_buffer > 0) {
        if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (const char *)&recv_buffer, sizeof(recv_buffer)) == SOCKET_ERROR_VALUE) {
            return NETWORK_ERROR_SOCKET;
        }
    }
    
    return NETWORK_SUCCESS;
}

// 初始化网络服务器
network_server *network_server_init(const network_config *config) {
    network_server *server = (network_server *)malloc(sizeof(network_server));
    if (!server) {
        return NULL;
    }
    
    // 设置默认值
    server->listen_socket = INVALID_SOCKET_VALUE;
    server->bind_address = config->bind_address ? strdup(config->bind_address) : strdup("0.0.0.0");
    server->port = config->port ? config->port : 3306;
    server->max_connections = config->max_connections ? config->max_connections : 1000;
    server->current_connections = 0;
    server->connections = NULL;
    server->running = false;
    server->initialized = false;
    server->ssl_enabled = config->ssl_enabled;
    server->ssl_cert = config->ssl_cert ? strdup(config->ssl_cert) : NULL;
    server->ssl_key = config->ssl_key ? strdup(config->ssl_key) : NULL;
    server->ssl_context = NULL;
    server->socket_timeout = config->socket_timeout ? config->socket_timeout : 300000;
    server->max_packet_size = config->max_packet_size ? config->max_packet_size : 1048576;
    
    // 初始化SSL（如果启用）
    if (server->ssl_enabled) {
        // SSL初始化代码将在后续实现
    }
    
    server->initialized = true;
    return server;
}

// 销毁网络服务器
void network_server_destroy(network_server *server) {
    if (!server) {
        return;
    }
    
    // 停止服务器
    network_server_stop(server);
    
    // 清理连接
    connection *current = server->connections;
    while (current) {
        connection *next = current->next;
        if (current->socket != INVALID_SOCKET) {
            closesocket(current->socket);
        }
        if (current->remote_address) {
            free(current->remote_address);
        }
        free(current);
        current = next;
    }
    
    // 清理SSL
    if (server->ssl_context) {
        // SSL清理代码
    }
    
    if (server->bind_address) {
        free(server->bind_address);
    }
    if (server->ssl_cert) {
        free(server->ssl_cert);
    }
    if (server->ssl_key) {
        free(server->ssl_key);
    }
    
    free(server);
}

// 启动网络服务器
network_error network_server_start(network_server *server) {
    if (!server || !server->initialized) {
        return NETWORK_ERROR_INIT;
    }
    
    // 创建监听套接字
    server->listen_socket = create_socket();
    if (server->listen_socket == INVALID_SOCKET_VALUE) {
        return NETWORK_ERROR_SOCKET;
    }
    
    // 设置套接字选项
    network_error error = set_socket_options(server->listen_socket, 8192, 8192);
    if (error != NETWORK_SUCCESS) {
#ifdef _WIN32
        closesocket(server->listen_socket);
#else
        close(server->listen_socket);
#endif
        server->listen_socket = INVALID_SOCKET_VALUE;
        return error;
    }
    
    // 设置地址结构体
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->port);
    
    if (strcmp(server->bind_address, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, server->bind_address, &addr.sin_addr);
    }
    
    // 绑定地址
    if (bind(server->listen_socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
        closesocket(server->listen_socket);
#else
        close(server->listen_socket);
#endif
        server->listen_socket = INVALID_SOCKET_VALUE;
        return NETWORK_ERROR_BIND;
    }
    
    // 开始监听
    if (listen(server->listen_socket, SOMAXCONN) == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
        closesocket(server->listen_socket);
#else
        close(server->listen_socket);
#endif
        server->listen_socket = INVALID_SOCKET_VALUE;
        return NETWORK_ERROR_LISTEN;
    }
    
    server->running = true;
    return NETWORK_SUCCESS;
}

// 停止网络服务器
void network_server_stop(network_server *server) {
    if (!server || !server->running) {
        return;
    }
    
    server->running = false;
    
    if (server->listen_socket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        closesocket(server->listen_socket);
#else
        close(server->listen_socket);
#endif
        server->listen_socket = INVALID_SOCKET_VALUE;
    }
}

// 接受新连接
connection *network_server_accept(network_server *server) {
    if (!server || !server->running || server->listen_socket == INVALID_SOCKET_VALUE) {
        return NULL;
    }
    
    if (server->current_connections >= server->max_connections) {
        return NULL;
    }
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    socket_t client_socket = accept(server->listen_socket, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket == INVALID_SOCKET_VALUE) {
        return NULL;
    }
    
    // 创建连接结构
    connection *conn = (connection *)malloc(sizeof(connection));
    if (!conn) {
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
        return NULL;
    }
    
    // 设置连接属性
    conn->socket = client_socket;
    conn->state = CONNECTION_STATE_CONNECTED;
    conn->id = global_connection_id++;
    conn->remote_address = (char *)malloc(INET_ADDRSTRLEN);
    if (conn->remote_address) {
        inet_ntop(AF_INET, &client_addr.sin_addr, conn->remote_address, INET_ADDRSTRLEN);
    }
    conn->remote_port = ntohs(client_addr.sin_port);
    conn->send_buffer_size = 8192;
    conn->recv_buffer_size = 8192;
    conn->ssl_enabled = server->ssl_enabled;
    conn->ssl_context = NULL;
    conn->last_activity = (uint64_t)time(NULL);
    conn->next = NULL;
    
    // 设置套接字选项
    set_socket_options(client_socket, conn->send_buffer_size, conn->recv_buffer_size);
    network_set_timeout(client_socket, server->socket_timeout);
    
    // 添加到连接列表
    if (!server->connections) {
        server->connections = conn;
    } else {
        connection *current = server->connections;
        while (current->next) {
            current = current->next;
        }
        current->next = conn;
    }
    
    server->current_connections++;
    return conn;
}

// 关闭连接
network_error network_server_close_connection(network_server *server, connection *conn) {
    if (!server || !conn) {
        return NETWORK_ERROR_INIT;
    }
    
    // 从连接列表中移除
    if (server->connections == conn) {
        server->connections = conn->next;
    } else {
        connection *current = server->connections;
        while (current && current->next != conn) {
            current = current->next;
        }
        if (current) {
            current->next = conn->next;
        }
    }
    
    // 关闭套接字
    if (conn->socket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        closesocket(conn->socket);
#else
        close(conn->socket);
#endif
    }
    
    // 清理资源
    if (conn->remote_address) {
        free(conn->remote_address);
    }
    if (conn->ssl_context) {
        // SSL清理代码
    }
    
    free(conn);
    server->current_connections--;
    return NETWORK_SUCCESS;
}

// 发送数据
network_error network_send(connection *conn, const void *data, uint32_t size) {
    if (!conn || !data || size == 0 || conn->state != CONNECTION_STATE_CONNECTED) {
        return NETWORK_ERROR_SEND;
    }
    
    int sent = send(conn->socket, (const char *)data, size, 0);
    if (sent == SOCKET_ERROR_VALUE) {
        return NETWORK_ERROR_SEND;
    }
    
    if (sent != size) {
        return NETWORK_ERROR_SEND;
    }
    
    conn->last_activity = (uint64_t)time(NULL);
    return NETWORK_SUCCESS;
}

// 接收数据
network_error network_receive(connection *conn, void *buffer, uint32_t buffer_size, uint32_t *received) {
    if (!conn || !buffer || !received || conn->state != CONNECTION_STATE_CONNECTED) {
        return NETWORK_ERROR_RECEIVE;
    }
    
    int recv_result = recv(conn->socket, (char *)buffer, buffer_size, 0);
    if (recv_result == SOCKET_ERROR_VALUE) {
        return NETWORK_ERROR_RECEIVE;
    }
    
    if (recv_result == 0) {
        return NETWORK_ERROR_CLOSE;
    }
    
    *received = (uint32_t)recv_result;
    conn->last_activity = (uint64_t)time(NULL);
    return NETWORK_SUCCESS;
}

// 初始化网络客户端
network_client *network_client_init(const char *address, uint16_t port, bool ssl_enabled) {
    network_client *client = (network_client *)malloc(sizeof(network_client));
    if (!client) {
        return NULL;
    }
    
    client->socket = INVALID_SOCKET_VALUE;
    client->state = CONNECTION_STATE_DISCONNECTED;
    client->server_address = address ? strdup(address) : strdup("127.0.0.1");
    client->server_port = port ? port : 3306;
    client->ssl_enabled = ssl_enabled;
    client->ssl_context = NULL;
    client->socket_timeout = 300000;
    client->max_packet_size = 1048576;
    
    // 初始化SSL（如果启用）
    if (client->ssl_enabled) {
        // SSL初始化代码将在后续实现
    }
    
    return client;
}

// 销毁网络客户端
void network_client_destroy(network_client *client) {
    if (!client) {
        return;
    }
    
    network_client_disconnect(client);
    
    if (client->server_address) {
        free(client->server_address);
    }
    if (client->ssl_context) {
        // SSL清理代码
    }
    
    free(client);
}

// 连接到服务器
network_error network_client_connect(network_client *client) {
    if (!client || client->state == CONNECTION_STATE_CONNECTED) {
        return NETWORK_ERROR_INIT;
    }
    
    // 创建套接字
    client->socket = create_socket();
    if (client->socket == INVALID_SOCKET_VALUE) {
        return NETWORK_ERROR_SOCKET;
    }
    
    // 设置套接字选项
    set_socket_options(client->socket, 8192, 8192);
    network_set_timeout(client->socket, client->socket_timeout);
    
    // 设置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client->server_port);
    
    if (inet_pton(AF_INET, client->server_address, &server_addr.sin_addr) != 1) {
#ifdef _WIN32
        closesocket(client->socket);
#else
        close(client->socket);
#endif
        client->socket = INVALID_SOCKET_VALUE;
        return NETWORK_ERROR_CONNECT;
    }
    
    // 连接到服务器
    client->state = CONNECTION_STATE_CONNECTING;
    if (connect(client->socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
        closesocket(client->socket);
#else
        close(client->socket);
#endif
        client->socket = INVALID_SOCKET_VALUE;
        client->state = CONNECTION_STATE_DISCONNECTED;
        return NETWORK_ERROR_CONNECT;
    }
    
    client->state = CONNECTION_STATE_CONNECTED;
    return NETWORK_SUCCESS;
}

// 断开连接
void network_client_disconnect(network_client *client) {
    if (!client) {
        return;
    }
    
    if (client->socket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        closesocket(client->socket);
#else
        close(client->socket);
#endif
        client->socket = INVALID_SOCKET_VALUE;
    }
    
    client->state = CONNECTION_STATE_DISCONNECTED;
}

// 客户端发送数据
network_error network_client_send(network_client *client, const void *data, uint32_t size) {
    if (!client || !data || size == 0 || client->state != CONNECTION_STATE_CONNECTED) {
        return NETWORK_ERROR_SEND;
    }
    
    int sent = send(client->socket, (const char *)data, size, 0);
    if (sent == SOCKET_ERROR_VALUE) {
        return NETWORK_ERROR_SEND;
    }
    
    if (sent != size) {
        return NETWORK_ERROR_SEND;
    }
    
    return NETWORK_SUCCESS;
}

// 客户端接收数据
network_error network_client_receive(network_client *client, void *buffer, uint32_t buffer_size, uint32_t *received) {
    if (!client || !buffer || !received || client->state != CONNECTION_STATE_CONNECTED) {
        return NETWORK_ERROR_RECEIVE;
    }
    
    int recv_result = recv(client->socket, (char *)buffer, buffer_size, 0);
    if (recv_result == SOCKET_ERROR_VALUE) {
        return NETWORK_ERROR_RECEIVE;
    }
    
    if (recv_result == 0) {
        return NETWORK_ERROR_CLOSE;
    }
    
    *received = (uint32_t)recv_result;
    return NETWORK_SUCCESS;
}

// 设置套接字超时
network_error network_set_timeout(socket_t socket, uint32_t timeout_ms) {
    if (socket == INVALID_SOCKET_VALUE) {
        return NETWORK_ERROR_SOCKET;
    }
    
#ifdef _WIN32
    DWORD timeout = timeout_ms;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) == SOCKET_ERROR_VALUE) {
        return NETWORK_ERROR_SOCKET;
    }
    
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout)) == SOCKET_ERROR_VALUE) {
        return NETWORK_ERROR_SOCKET;
    }
#else
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) == SOCKET_ERROR_VALUE) {
        return NETWORK_ERROR_SOCKET;
    }
    
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout)) == SOCKET_ERROR_VALUE) {
        return NETWORK_ERROR_SOCKET;
    }
#endif
    
    return NETWORK_SUCCESS;
}

// 获取连接状态
connection_state network_get_connection_state(connection *conn) {
    if (!conn) {
        return CONNECTION_STATE_DISCONNECTED;
    }
    return conn->state;
}

// 获取连接地址和端口
void network_get_connection_info(connection *conn, char **address, uint16_t *port) {
    if (!conn) {
        if (address) {
            *address = NULL;
        }
        if (port) {
            *port = 0;
        }
        return;
    }
    
    if (address) {
        *address = conn->remote_address;
    }
    if (port) {
        *port = conn->remote_port;
    }
}

// 获取服务器当前连接数
uint32_t network_server_get_connections(network_server *server) {
    if (!server) {
        return 0;
    }
    return server->current_connections;
}

#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>

// 跨平台兼容性定义
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
#endif

// 网络模块错误码
typedef enum {
    NETWORK_SUCCESS = 0,
    NETWORK_ERROR_INIT = 1,
    NETWORK_ERROR_SOCKET = 2,
    NETWORK_ERROR_BIND = 3,
    NETWORK_ERROR_LISTEN = 4,
    NETWORK_ERROR_ACCEPT = 5,
    NETWORK_ERROR_CONNECT = 6,
    NETWORK_ERROR_SEND = 7,
    NETWORK_ERROR_RECEIVE = 8,
    NETWORK_ERROR_CLOSE = 9,
    NETWORK_ERROR_TIMEOUT = 10,
    NETWORK_ERROR_SSL = 11
} network_error;

// 连接状态
typedef enum {
    CONNECTION_STATE_DISCONNECTED,
    CONNECTION_STATE_CONNECTING,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_CLOSING
} connection_state;

// 连接结构
typedef struct {
    socket_t socket;
    connection_state state;
    uint32_t id;
    char *remote_address;
    uint16_t remote_port;
    uint32_t send_buffer_size;
    uint32_t recv_buffer_size;
    bool ssl_enabled;
    void *ssl_context;
    uint64_t last_activity;
    struct connection *next;
} connection;

// 网络服务器结构
typedef struct {
    socket_t listen_socket;
    char *bind_address;
    uint16_t port;
    uint32_t max_connections;
    uint32_t current_connections;
    connection *connections;
    bool running;
    bool initialized;
    bool ssl_enabled;
    char *ssl_cert;
    char *ssl_key;
    void *ssl_context;
    uint32_t socket_timeout;
    uint32_t max_packet_size;
} network_server;

// 网络客户端结构
typedef struct {
    socket_t socket;
    connection_state state;
    char *server_address;
    uint16_t server_port;
    bool ssl_enabled;
    void *ssl_context;
    uint32_t socket_timeout;
    uint32_t max_packet_size;
} network_client;

// 网络配置结构
typedef struct {
    char *bind_address;
    uint16_t port;
    uint32_t max_connections;
    bool ssl_enabled;
    char *ssl_cert;
    char *ssl_key;
    uint32_t socket_timeout;
    uint32_t max_packet_size;
} network_config;

// 初始化Winsock
network_error network_init(void);

// 清理Winsock
void network_cleanup(void);

// 初始化网络服务器
network_server *network_server_init(const network_config *config);

// 销毁网络服务器
void network_server_destroy(network_server *server);

// 启动网络服务器
network_error network_server_start(network_server *server);

// 停止网络服务器
void network_server_stop(network_server *server);

// 接受新连接
connection *network_server_accept(network_server *server);

// 关闭连接
network_error network_server_close_connection(network_server *server, connection *conn);

// 发送数据
network_error network_send(connection *conn, const void *data, uint32_t size);

// 接收数据
network_error network_receive(connection *conn, void *buffer, uint32_t buffer_size, uint32_t *received);

// 初始化网络客户端
network_client *network_client_init(const char *address, uint16_t port, bool ssl_enabled);

// 销毁网络客户端
void network_client_destroy(network_client *client);

// 连接到服务器
network_error network_client_connect(network_client *client);

// 断开连接
void network_client_disconnect(network_client *client);

// 客户端发送数据
network_error network_client_send(network_client *client, const void *data, uint32_t size);

// 客户端接收数据
network_error network_client_receive(network_client *client, void *buffer, uint32_t buffer_size, uint32_t *received);

// 设置套接字超时
network_error network_set_timeout(SOCKET socket, uint32_t timeout_ms);

// 获取连接状态
connection_state network_get_connection_state(connection *conn);

// 获取连接地址和端口
void network_get_connection_info(connection *conn, char **address, uint16_t *port);

// 获取服务器当前连接数
uint32_t network_server_get_connections(network_server *server);

#endif // NETWORK_H

#include "system/system.h"
#include "config/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGUSR1
#define SIGUSR1 2
#endif
#ifndef SIGUSR2
#define SIGUSR2 3
#endif
#ifndef SIGINT
#define SIGINT 2
#endif
#ifndef SIGTERM
#define SIGTERM 15
#endif
#ifndef getpid
#define getpid() 0
#endif
#ifndef pause
#define pause() Sleep(1000)
#endif
#endif

// 命令行参数结构
typedef struct {
    char *config_file;
    char *data_dir;
    char *log_dir;
    bool daemonize;
    char *pid_file;
    bool help;
} cmd_args;

// 打印帮助信息
static void print_help(const char *prog_name) {
    printf("MicroMeowDB - A lightweight database management system\n");
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -c, --config FILE     Specify configuration file (default: micromeow.conf)\n");
    printf("  -d, --datadir DIR     Specify data directory (default: ./data)\n");
    printf("  -l, --logdir DIR      Specify log directory (default: ./logs)\n");
    printf("  -D, --daemonize       Run as daemon (Unix/Linux only)\n");
    printf("  -p, --pidfile FILE    Specify PID file\n");
    printf("  -h, --help            Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                          Start with default settings\n", prog_name);
    printf("  %s -c /etc/mm.conf         Use custom configuration file\n", prog_name);
    printf("  %s -d /var/lib/mm -l /var/log/mm  Custom data and log directories\n", prog_name);
}

// 解析命令行参数
static cmd_args *parse_cmd_args(int argc, char *argv[]) {
    cmd_args *args = (cmd_args *)malloc(sizeof(cmd_args));
    if (!args) {
        return NULL;
    }
    
    // 初始化默认值
    args->config_file = NULL;  // 使用默认配置文件
    args->data_dir = NULL;     // 使用默认数据目录
    args->log_dir = NULL;      // 使用默认日志目录
    args->daemonize = false;
    args->pid_file = NULL;
    args->help = false;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                args->config_file = argv[i + 1];
                i++;
            } else {
                fprintf(stderr, "Error: Configuration file path required\n");
                free(args);
                return NULL;
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--datadir") == 0) {
            if (i + 1 < argc) {
                args->data_dir = argv[i + 1];
                i++;
            } else {
                fprintf(stderr, "Error: Data directory path required\n");
                free(args);
                return NULL;
            }
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--logdir") == 0) {
            if (i + 1 < argc) {
                args->log_dir = argv[i + 1];
                i++;
            } else {
                fprintf(stderr, "Error: Log directory path required\n");
                free(args);
                return NULL;
            }
        } else if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--daemonize") == 0) {
            args->daemonize = true;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pidfile") == 0) {
            if (i + 1 < argc) {
                args->pid_file = argv[i + 1];
                i++;
            } else {
                fprintf(stderr, "Error: PID file path required\n");
                free(args);
                return NULL;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            args->help = true;
        } else {
            fprintf(stderr, "Warning: Unknown option '%s'\n", argv[i]);
        }
    }
    
    return args;
}

// 守护进程化 (仅限 Unix/Linux)
static bool daemonize_process() {
#ifdef _WIN32
    fprintf(stderr, "Warning: Daemon mode is not supported on Windows\n");
    return false;
#else
    // 实现守护进程化逻辑
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return false;
    }
    
    if (pid > 0) {
        // 父进程退出
        exit(EXIT_SUCCESS);
    }
    
    // 子进程成为会话领袖
    if (setsid() < 0) {
        perror("setsid failed");
        return false;
    }
    
    // 忽略终端I/O信号
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    
    // 第二次fork确保不是会话领袖
    pid = fork();
    if (pid < 0) {
        perror("second fork failed");
        return false;
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // 改变工作目录到根目录
    if (chdir("/") < 0) {
        perror("chdir failed");
        return false;
    }
    
    // 重设文件权限掩码
    umask(0);
    
    // 关闭所有打开的文件描述符
    for (int fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) {
        close(fd);
    }
    
    // 重定向标准文件描述符到/dev/null
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    
    return true;
#endif
}

// 写入PID文件
static bool write_pid_file(const char *pid_file) {
    if (!pid_file) {
        return true;
    }
    
    FILE *fp = fopen(pid_file, "w");
    if (!fp) {
        perror("Failed to open PID file");
        return false;
    }
    
    fprintf(fp, "%d\n", getpid());
    fclose(fp);
    
    return true;
}

// 移除PID文件
static void remove_pid_file(const char *pid_file) {
    if (pid_file) {
        if (remove(pid_file) != 0) {
            perror("Failed to remove PID file");
        }
    }
}

// 设置信号处理函数
static void setup_signal_handlers(void) {
    // 设置信号处理器
    signal(SIGINT, mm_system_handle_signal);
    signal(SIGTERM, mm_system_handle_signal);
    
#ifdef _WIN32
    // Windows 不支持 SIGHUP, SIGUSR1, SIGUSR2
    printf("Note: Signal handlers SIGHUP, SIGUSR1, SIGUSR2 are not supported on Windows\n");
#else
    signal(SIGHUP, mm_system_handle_signal);
    signal(SIGUSR1, mm_system_handle_signal);
    signal(SIGUSR2, mm_system_handle_signal);
#endif
}

// 初始化系统配置
static mm_system_config *init_system_config(const cmd_args *args) {
    mm_system_config *config = (mm_system_config *)malloc(sizeof(mm_system_config));
    if (!config) {
        return NULL;
    }
    
    // 设置配置值，如果命令行参数为NULL则使用默认值
    config->config_file = args->config_file ? args->config_file : "micromeow.conf";
    config->data_dir = args->data_dir ? args->data_dir : "./data";
    config->log_dir = args->log_dir ? args->log_dir : "./logs";
    config->daemonize = args->daemonize;
    config->pid_file = args->pid_file;
    
    return config;
}

// 主函数
int main(int argc, char *argv[]) {
    printf("MicroMeowDB Starting...\n");
    
    // 解析命令行参数
    cmd_args *args = parse_cmd_args(argc, argv);
    if (!args) {
        fprintf(stderr, "Failed to parse command line arguments\n");
        return EXIT_FAILURE;
    }
    
    // 打印帮助信息
    if (args->help) {
        print_help(argv[0]);
        free(args);
        return EXIT_SUCCESS;
    }
    
    // 显示启动信息
    printf("Configuration file: %s\n", args->config_file ? args->config_file : "micromeow.conf (default)");
    printf("Data directory: %s\n", args->data_dir ? args->data_dir : "./data (default)");
    printf("Log directory: %s\n", args->log_dir ? args->log_dir : "./logs (default)");
    printf("Daemon mode: %s\n", args->daemonize ? "enabled" : "disabled");
    if (args->pid_file) {
        printf("PID file: %s\n", args->pid_file);
    }
    
    // 守护进程化 (仅在非Windows系统且启用时)
    if (args->daemonize) {
        if (!daemonize_process()) {
            fprintf(stderr, "Failed to daemonize process\n");
            free(args);
            return EXIT_FAILURE;
        }
        // 守护进程模式下不输出到控制台
        printf("Running in daemon mode...\n");
    }
    
    // 写入PID文件
    if (!write_pid_file(args->pid_file)) {
        fprintf(stderr, "Failed to write PID file\n");
        free(args);
        return EXIT_FAILURE;
    }
    
    // 初始化系统配置
    mm_system_config *config = init_system_config(args);
    if (!config) {
        fprintf(stderr, "Failed to initialize system configuration\n");
        remove_pid_file(args->pid_file);
        free(args);
        return EXIT_FAILURE;
    }
    
    // 初始化系统管理器
    mm_system_manager *system_mgr = mm_system_init(config);
    if (!system_mgr) {
        fprintf(stderr, "Failed to initialize system manager\n");
        free(config);
        remove_pid_file(args->pid_file);
        free(args);
        return EXIT_FAILURE;
    }
    
    // 设置信号处理
    setup_signal_handlers();
    
    printf("System manager initialized successfully\n");
    
    // 启动系统
    if (!mm_system_start(system_mgr)) {
        fprintf(stderr, "Failed to start system\n");
        mm_system_destroy(system_mgr);
        free(config);
        remove_pid_file(args->pid_file);
        free(args);
        return EXIT_FAILURE;
    }
    
    printf("MicroMeowDB started successfully\n");
    printf("System state: RUNNING\n");
    printf("Process ID: %d\n", getpid());
    
    // 等待信号
    while (mm_system_get_state(system_mgr) == MM_SYSTEM_STATE_RUNNING) {
        pause();
    }
    
    printf("Received shutdown signal, stopping system...\n");
    
    // 销毁系统
    mm_system_destroy(system_mgr);
    printf("System destroyed\n");
    
    // 释放配置
    free(config);
    
    // 移除PID文件
    remove_pid_file(args->pid_file);
    
    // 释放命令行参数
    free(args);
    
    printf("MicroMeowDB stopped successfully\n");
    
    return EXIT_SUCCESS;
}

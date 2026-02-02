#include "system/system.h"
#include "config/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

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
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -c, --config FILE     Specify configuration file\n");
    printf("  -d, --datadir DIR     Specify data directory\n");
    printf("  -l, --logdir DIR      Specify log directory\n");
    printf("  -D, --daemonize       Run as daemon\n");
    printf("  -p, --pidfile FILE    Specify PID file\n");
    printf("  -h, --help            Show this help message\n");
}

// 解析命令行参数
static cmd_args *parse_cmd_args(int argc, char *argv[]) {
    cmd_args *args = (cmd_args *)malloc(sizeof(cmd_args));
    if (!args) {
        return NULL;
    }
    
    // 初始化默认值
    args->config_file = NULL;
    args->data_dir = NULL;
    args->log_dir = NULL;
    args->daemonize = false;
    args->pid_file = NULL;
    args->help = false;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                args->config_file = argv[i + 1];
                i++;
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--datadir") == 0) {
            if (i + 1 < argc) {
                args->data_dir = argv[i + 1];
                i++;
            }
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--logdir") == 0) {
            if (i + 1 < argc) {
                args->log_dir = argv[i + 1];
                i++;
            }
        } else if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--daemonize") == 0) {
            args->daemonize = true;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pidfile") == 0) {
            if (i + 1 < argc) {
                args->pid_file = argv[i + 1];
                i++;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            args->help = true;
        }
    }
    
    return args;
}

// 守护进程化
static bool daemonize_process() {
    // 实现守护进程化逻辑
    // 这里只是一个简单的实现，实际生产环境中可能需要更复杂的处理
    return true;
}

// 写入PID文件
static bool write_pid_file(const char *pid_file) {
    if (!pid_file) {
        return true;
    }
    
    FILE *fp = fopen(pid_file, "w");
    if (!fp) {
        return false;
    }
    
    fprintf(fp, "%d\n", getpid());
    fclose(fp);
    
    return true;
}

// 移除PID文件
static void remove_pid_file(const char *pid_file) {
    if (pid_file) {
        remove(pid_file);
    }
}

// 主函数
int main(int argc, char *argv[]) {
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
    
    // 守护进程化
    if (args->daemonize) {
        if (!daemonize_process()) {
            fprintf(stderr, "Failed to daemonize process\n");
            free(args);
            return EXIT_FAILURE;
        }
    }
    
    // 写入PID文件
    if (!write_pid_file(args->pid_file)) {
        fprintf(stderr, "Failed to write PID file\n");
        free(args);
        return EXIT_FAILURE;
    }
    
    // 初始化系统配置
    system_config config;
    config.config_file = args->config_file;
    config.data_dir = args->data_dir;
    config.log_dir = args->log_dir;
    config.daemonize = args->daemonize;
    config.pid_file = args->pid_file;
    
    // 初始化系统管理器
    system_manager *system = system_init(&config);
    if (!system) {
        fprintf(stderr, "Failed to initialize system manager\n");
        remove_pid_file(args->pid_file);
        free(args);
        return EXIT_FAILURE;
    }
    
    // 注册信号处理
    signal(SIGINT, system_handle_signal);
    signal(SIGTERM, system_handle_signal);
    signal(SIGHUP, system_handle_signal);
    signal(SIGUSR1, system_handle_signal);
    signal(SIGUSR2, system_handle_signal);
    
    // 启动系统
    if (!system_start(system)) {
        fprintf(stderr, "Failed to start system\n");
        system_destroy(system);
        remove_pid_file(args->pid_file);
        free(args);
        return EXIT_FAILURE;
    }
    
    printf("MicroMeowDB started successfully\n");
    
    // 等待信号
    while (system_get_state(system) == SYSTEM_STATE_RUNNING) {
        pause();
    }
    
    // 销毁系统
    system_destroy(system);
    
    // 移除PID文件
    remove_pid_file(args->pid_file);
    
    // 释放命令行参数
    free(args);
    
    printf("MicroMeowDB stopped\n");
    
    return EXIT_SUCCESS;
}

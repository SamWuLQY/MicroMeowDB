#include "path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

// 连接两个路径
char* path_join(const char* path1, const char* path2) {
    if (!path1 || !path2) {
        return NULL;
    }

    size_t path1_len = strlen(path1);
    size_t path2_len = strlen(path2);
    size_t result_len = path1_len + path2_len + 2; // +1 for separator, +1 for null terminator

    char* result = (char*)malloc(result_len);
    if (!result) {
        return NULL;
    }

    strcpy(result, path1);

    // 检查path1是否以路径分隔符结尾
    if (path1_len > 0 && result[path1_len - 1] != PATH_SEPARATOR) {
        strcat(result, PATH_SEPARATOR_STR);
    }

    // 检查path2是否以路径分隔符开头
    if (path2_len > 0 && path2[0] == PATH_SEPARATOR) {
        strcat(result, path2 + 1);
    } else {
        strcat(result, path2);
    }

    return result;
}

// 规范化路径
char* path_normalize(const char* path) {
    if (!path) {
        return NULL;
    }

    char* result = strdup(path);
    if (!result) {
        return NULL;
    }

    // 替换所有的'/'为PATH_SEPARATOR（在Windows上）
#ifdef _WIN32
    char* p = result;
    while (*p) {
        if (*p == '/') {
            *p = PATH_SEPARATOR;
        }
        p++;
    }
#endif

    return result;
}

// 获取路径的基名
char* path_basename(const char* path) {
    if (!path) {
        return NULL;
    }

    const char* last_sep = strrchr(path, PATH_SEPARATOR);
    if (!last_sep) {
        return strdup(path);
    }

    return strdup(last_sep + 1);
}

// 获取路径的目录名
char* path_dirname(const char* path) {
    if (!path) {
        return NULL;
    }

    const char* last_sep = strrchr(path, PATH_SEPARATOR);
    if (!last_sep) {
        return strdup(".");
    }

    size_t len = last_sep - path;
    char* result = (char*)malloc(len + 1);
    if (!result) {
        return NULL;
    }

    strncpy(result, path, len);
    result[len] = '\0';

    return result;
}

// 检查路径是否存在
bool path_exists(const char* path) {
    if (!path) {
        return false;
    }

#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    struct stat stat_buf;
    return stat(path, &stat_buf) == 0;
#endif
}

// 检查路径是否为绝对路径
bool path_is_absolute(const char* path) {
    if (!path) {
        return false;
    }

#ifdef _WIN32
    // Windows绝对路径格式: C:\path 或 \\server\share
    if (strlen(path) >= 2 && isalpha(path[0]) && path[1] == ':') {
        return true;
    }
    if (strlen(path) >= 2 && path[0] == '\\' && path[1] == '\\') {
        return true;
    }
    return false;
#else
    // Unix绝对路径格式: /path
    return path[0] == '/';
#endif
}

// 获取绝对路径
char* path_absolute(const char* path) {
    if (!path) {
        return NULL;
    }

    if (path_is_absolute(path)) {
        return strdup(path);
    }

#ifdef _WIN32
    char* result = (char*)malloc(MAX_PATH);
    if (!result) {
        return NULL;
    }

    if (GetFullPathNameA(path, MAX_PATH, result, NULL) == 0) {
        free(result);
        return NULL;
    }

    return result;
#else
    char* result = realpath(path, NULL);
    if (!result) {
        // 如果realpath失败，尝试使用当前目录
        char* cwd = getcwd(NULL, 0);
        if (!cwd) {
            return NULL;
        }

        result = path_join(cwd, path);
        free(cwd);
    }

    return result;
#endif
}

#ifndef PATH_H
#define PATH_H

#include <stdint.h>
#include <stdbool.h>

// 路径分隔符
#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#endif

// 路径处理函数
char* path_join(const char* path1, const char* path2);
char* path_normalize(const char* path);
char* path_basename(const char* path);
char* path_dirname(const char* path);
bool path_exists(const char* path);
bool path_is_absolute(const char* path);
char* path_absolute(const char* path);

#endif // PATH_H
#ifndef TEST_H
#define TEST_H

#include "../src/config/config.h"
#include "../src/error/error.h"
#include "../src/logging/logging.h"

#define TEST_MODULE "test"

#define MAX_TEST_NAME_LENGTH 256
#define MAX_TESTS_PER_SUITE 100
#define MAX_TEST_SUITES 50

#define TEST_STATUS_PASS 1
#define TEST_STATUS_FAIL 2
#define TEST_STATUS_SKIP 3
#define TEST_STATUS_ERROR 4

// 测试函数类型
typedef int (*test_func)(void);

// 测试用例结构
typedef struct {
    char *name;
    test_func func;
    int status;
    int error_code;
    char *error_message;
    uint64_t duration;
} test_case;

// 测试套件结构
typedef struct {
    char *name;
    test_case *tests;
    int test_count;
    int passed;
    int failed;
    int skipped;
    int errors;
    uint64_t total_duration;
} test_suite;

// 测试运行器结构
typedef struct {
    test_suite *suites;
    int suite_count;
    int total_passed;
    int total_failed;
    int total_skipped;
    int total_errors;
    uint64_t total_duration;
    bool verbose;
    bool quiet;
    bool exit_on_fail;
} test_runner;

// 测试结果结构
typedef struct {
    int passed;
    int failed;
    int skipped;
    int errors;
    uint64_t duration;
    bool success;
} test_result;

// 初始化测试运行器
test_runner *test_runner_create(bool verbose, bool quiet, bool exit_on_fail);
void test_runner_destroy(test_runner *runner);

// 添加测试套件
test_suite *test_runner_add_suite(test_runner *runner, const char *name);

// 添加测试用例
test_case *test_suite_add_test(test_suite *suite, const char *name, test_func func);

// 运行测试
int test_runner_run(test_runner *runner);

// 运行单个测试套件
int test_suite_run(test_suite *suite, bool verbose);

// 运行单个测试用例
int test_case_run(test_case *test, bool verbose);

// 打印测试结果
void test_runner_print_results(test_runner *runner);
void test_suite_print_results(test_suite *suite);
void test_case_print_result(test_case *test);

// 测试辅助函数
int test_assert_true(bool condition, const char *message);
int test_assert_false(bool condition, const char *message);
int test_assert_equal(int a, int b, const char *message);
int test_assert_not_equal(int a, int b, const char *message);
int test_assert_null(void *ptr, const char *message);
int test_assert_not_null(void *ptr, const char *message);
int test_assert_str_equal(const char *a, const char *b, const char *message);
int test_assert_str_not_equal(const char *a, const char *b, const char *message);

// 测试配置
#define TEST_CONFIG_FILE "./test_config.ini"

#endif
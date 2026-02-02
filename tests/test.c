#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t get_timestamp(void);
static void print_test_status(int status);

// 获取时间戳
static uint64_t get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// 打印测试状态
static void print_test_status(int status) {
    switch (status) {
        case TEST_STATUS_PASS:
            printf("\033[32mPASS\033[0m");
            break;
        case TEST_STATUS_FAIL:
            printf("\033[31mFAIL\033[0m");
            break;
        case TEST_STATUS_SKIP:
            printf("\033[33mSKIP\033[0m");
            break;
        case TEST_STATUS_ERROR:
            printf("\033[31mERROR\033[0m");
            break;
        default:
            printf("\033[37mUNKNOWN\033[0m");
            break;
    }
}

// 初始化测试运行器
test_runner *test_runner_create(bool verbose, bool quiet, bool exit_on_fail) {
    test_runner *runner = (test_runner *)malloc(sizeof(test_runner));
    if (!runner) {
        return NULL;
    }

    memset(runner, 0, sizeof(test_runner));
    runner->suites = (test_suite *)malloc(MAX_TEST_SUITES * sizeof(test_suite));
    if (!runner->suites) {
        free(runner);
        return NULL;
    }

    runner->verbose = verbose;
    runner->quiet = quiet;
    runner->exit_on_fail = exit_on_fail;
    runner->suite_count = 0;
    runner->total_passed = 0;
    runner->total_failed = 0;
    runner->total_skipped = 0;
    runner->total_errors = 0;
    runner->total_duration = 0;

    return runner;
}

// 销毁测试运行器
void test_runner_destroy(test_runner *runner) {
    if (runner) {
        for (int i = 0; i < runner->suite_count; i++) {
            test_suite *suite = &runner->suites[i];
            if (suite->name) {
                free(suite->name);
            }
            for (int j = 0; j < suite->test_count; j++) {
                test_case *test = &suite->tests[j];
                if (test->name) {
                    free(test->name);
                }
                if (test->error_message) {
                    free(test->error_message);
                }
            }
            if (suite->tests) {
                free(suite->tests);
            }
        }
        if (runner->suites) {
            free(runner->suites);
        }
        free(runner);
    }
}

// 添加测试套件
test_suite *test_runner_add_suite(test_runner *runner, const char *name) {
    if (!runner || !name || runner->suite_count >= MAX_TEST_SUITES) {
        return NULL;
    }

    test_suite *suite = &runner->suites[runner->suite_count++];
    suite->name = strdup(name);
    suite->tests = (test_case *)malloc(MAX_TESTS_PER_SUITE * sizeof(test_case));
    if (!suite->tests) {
        free(suite->name);
        runner->suite_count--;
        return NULL;
    }
    suite->test_count = 0;
    suite->passed = 0;
    suite->failed = 0;
    suite->skipped = 0;
    suite->errors = 0;
    suite->total_duration = 0;

    return suite;
}

// 添加测试用例
test_case *test_suite_add_test(test_suite *suite, const char *name, test_func func) {
    if (!suite || !name || !func || suite->test_count >= MAX_TESTS_PER_SUITE) {
        return NULL;
    }

    test_case *test = &suite->tests[suite->test_count++];
    test->name = strdup(name);
    test->func = func;
    test->status = 0;
    test->error_code = 0;
    test->error_message = NULL;
    test->duration = 0;

    return test;
}

// 运行测试
int test_runner_run(test_runner *runner) {
    if (!runner) {
        return ERROR_INVALID_PARAMETER;
    }

    uint64_t start_time = get_timestamp();

    for (int i = 0; i < runner->suite_count; i++) {
        test_suite *suite = &runner->suites[i];
        int result = test_suite_run(suite, runner->verbose);
        if (result != SUCCESS && runner->exit_on_fail) {
            break;
        }
    }

    runner->total_duration = get_timestamp() - start_time;

    test_runner_print_results(runner);

    return runner->total_failed == 0 && runner->total_errors == 0 ? SUCCESS : ERROR_OPERATION_FAILED;
}

// 运行单个测试套件
int test_suite_run(test_suite *suite, bool verbose) {
    if (!suite) {
        return ERROR_INVALID_PARAMETER;
    }

    uint64_t start_time = get_timestamp();

    if (!verbose) {
        printf("Running suite: %s... ", suite->name);
        fflush(stdout);
    }

    for (int i = 0; i < suite->test_count; i++) {
        test_case *test = &suite->tests[i];
        int result = test_case_run(test, verbose);
        if (result != SUCCESS) {
            suite->errors++;
        } else {
            switch (test->status) {
                case TEST_STATUS_PASS:
                    suite->passed++;
                    break;
                case TEST_STATUS_FAIL:
                    suite->failed++;
                    break;
                case TEST_STATUS_SKIP:
                    suite->skipped++;
                    break;
                case TEST_STATUS_ERROR:
                    suite->errors++;
                    break;
            }
        }
    }

    suite->total_duration = get_timestamp() - start_time;

    if (!verbose) {
        print_test_status(suite->failed == 0 && suite->errors == 0 ? TEST_STATUS_PASS : TEST_STATUS_FAIL);
        printf("\n");
    } else {
        test_suite_print_results(suite);
    }

    return suite->failed == 0 && suite->errors == 0 ? SUCCESS : ERROR_OPERATION_FAILED;
}

// 运行单个测试用例
int test_case_run(test_case *test, bool verbose) {
    if (!test || !test->func) {
        return ERROR_INVALID_PARAMETER;
    }

    uint64_t start_time = get_timestamp();

    if (verbose) {
        printf("  Running test: %s... ", test->name);
        fflush(stdout);
    }

    int result = test->func();

    test->duration = get_timestamp() - start_time;

    if (result == SUCCESS) {
        test->status = TEST_STATUS_PASS;
    } else if (result == ERROR_SKIP) {
        test->status = TEST_STATUS_SKIP;
    } else if (result == ERROR_FAIL) {
        test->status = TEST_STATUS_FAIL;
    } else {
        test->status = TEST_STATUS_ERROR;
        test->error_code = result;
        test->error_message = strdup(error_get_description(result));
    }

    if (verbose) {
        print_test_status(test->status);
        printf(" (%llu ms)\n", (unsigned long long)test->duration);
        if (test->status == TEST_STATUS_ERROR) {
            printf("    Error: %s\n", test->error_message);
        }
    }

    return SUCCESS;
}

// 打印测试结果
void test_runner_print_results(test_runner *runner) {
    if (!runner || runner->quiet) {
        return;
    }

    printf("\n====================================\n");
    printf("Test Results\n");
    printf("====================================\n");

    for (int i = 0; i < runner->suite_count; i++) {
        test_suite *suite = &runner->suites[i];
        test_suite_print_results(suite);
    }

    printf("====================================\n");
    printf("Total: %d tests\n", runner->total_passed + runner->total_failed + runner->total_skipped + runner->total_errors);
    printf("Passed: %d\n", runner->total_passed);
    printf("Failed: %d\n", runner->total_failed);
    printf("Skipped: %d\n", runner->total_skipped);
    printf("Errors: %d\n", runner->total_errors);
    printf("Duration: %llu ms\n", (unsigned long long)runner->total_duration);
    printf("====================================\n");

    if (runner->total_failed == 0 && runner->total_errors == 0) {
        printf("\033[32mAll tests passed!\033[0m\n");
    } else {
        printf("\033[31mSome tests failed!\033[0m\n");
    }
}

// 打印测试套件结果
void test_suite_print_results(test_suite *suite) {
    if (!suite) {
        return;
    }

    printf("Suite: %s\n", suite->name);
    printf("  Tests: %d\n", suite->test_count);
    printf("  Passed: %d\n", suite->passed);
    printf("  Failed: %d\n", suite->failed);
    printf("  Skipped: %d\n", suite->skipped);
    printf("  Errors: %d\n", suite->errors);
    printf("  Duration: %llu ms\n", (unsigned long long)suite->total_duration);
    printf("\n");
}

// 打印测试用例结果
void test_case_print_result(test_case *test) {
    if (!test) {
        return;
    }

    printf("Test: %s\n", test->name);
    printf("  Status: ");
    print_test_status(test->status);
    printf("\n");
    if (test->status == TEST_STATUS_ERROR) {
        printf("  Error: %s\n", test->error_message);
    }
    printf("  Duration: %llu ms\n", (unsigned long long)test->duration);
    printf("\n");
}

// 测试辅助函数
int test_assert_true(bool condition, const char *message) {
    if (!condition) {
        if (message) {
            printf("Assertion failed: %s\n", message);
        }
        return ERROR_FAIL;
    }
    return SUCCESS;
}

int test_assert_false(bool condition, const char *message) {
    if (condition) {
        if (message) {
            printf("Assertion failed: %s\n", message);
        }
        return ERROR_FAIL;
    }
    return SUCCESS;
}

int test_assert_equal(int a, int b, const char *message) {
    if (a != b) {
        if (message) {
            printf("Assertion failed: %s\n", message);
        }
        printf("Expected: %d, Actual: %d\n", a, b);
        return ERROR_FAIL;
    }
    return SUCCESS;
}

int test_assert_not_equal(int a, int b, const char *message) {
    if (a == b) {
        if (message) {
            printf("Assertion failed: %s\n", message);
        }
        printf("Expected: not %d, Actual: %d\n", a, b);
        return ERROR_FAIL;
    }
    return SUCCESS;
}

int test_assert_null(void *ptr, const char *message) {
    if (ptr != NULL) {
        if (message) {
            printf("Assertion failed: %s\n", message);
        }
        printf("Expected: NULL, Actual: %p\n", ptr);
        return ERROR_FAIL;
    }
    return SUCCESS;
}

int test_assert_not_null(void *ptr, const char *message) {
    if (ptr == NULL) {
        if (message) {
            printf("Assertion failed: %s\n", message);
        }
        printf("Expected: not NULL, Actual: NULL\n");
        return ERROR_FAIL;
    }
    return SUCCESS;
}

int test_assert_str_equal(const char *a, const char *b, const char *message) {
    if (strcmp(a, b) != 0) {
        if (message) {
            printf("Assertion failed: %s\n", message);
        }
        printf("Expected: '%s', Actual: '%s'\n", a, b);
        return ERROR_FAIL;
    }
    return SUCCESS;
}

int test_assert_str_not_equal(const char *a, const char *b, const char *message) {
    if (strcmp(a, b) == 0) {
        if (message) {
            printf("Assertion failed: %s\n", message);
        }
        printf("Expected: not '%s', Actual: '%s'\n", a, b);
        return ERROR_FAIL;
    }
    return SUCCESS;
}

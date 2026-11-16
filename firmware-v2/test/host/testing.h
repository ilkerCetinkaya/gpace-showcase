#ifndef TESTING_H
#define TESTING_H

#include <stdio.h>
#include <stdlib.h>

extern int g_checks;
extern int g_failures;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        g_checks++;                                                                   \
        if (!(cond)) {                                                                \
            g_failures++;                                                             \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                  \
        }                                                                             \
    } while (0)

#define CHECK_EQ(actual, expected)                                                    \
    do {                                                                              \
        long long a_ = (long long)(actual);                                           \
        long long e_ = (long long)(expected);                                         \
        g_checks++;                                                                   \
        if (a_ != e_) {                                                               \
            g_failures++;                                                             \
            printf("  FAIL %s:%d: %s = %lld, expected %lld\n", __FILE__, __LINE__,    \
                   #actual, a_, e_);                                                  \
        }                                                                             \
    } while (0)

#define RUN(test)                                                                     \
    do {                                                                              \
        int before_ = g_failures;                                                     \
        test();                                                                       \
        printf("%s %s\n", g_failures == before_ ? "ok  " : "FAIL", #test);            \
    } while (0)

void run_ubx_tests(void);
void run_session_tests(void);
void run_bmi270_tests(void);
void run_frame_tests(void);
void run_battery_tests(void);

#endif

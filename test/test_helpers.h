#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void test_helpers_reset_counts(void);
void pass(const char *name);
void fail(const char *name, const char *detail);
bool expect_true(const char *name, bool ok, const char *detail);
uint32_t test_helpers_pass_count(void);
uint32_t test_helpers_fail_count(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_HELPERS_H */

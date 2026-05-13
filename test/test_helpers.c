#include "test_helpers.h"

#include <stdio.h>

static uint32_t g_pass_count;
static uint32_t g_fail_count;

void test_helpers_reset_counts(void)
{
    g_pass_count = 0U;
    g_fail_count = 0U;
}

void pass(const char *name)
{
    ++g_pass_count;
    printf("TEST PASSED: %s\n", name);
}

void fail(const char *name, const char *detail)
{
    ++g_fail_count;
    printf("TEST FAILED: %s (%s)\n", name, detail ? detail : "no detail");
}

bool expect_true(const char *name, bool ok, const char *detail)
{
    if (!ok) {
        fail(name, detail);
        return false;
    }

    pass(name);
    return true;
}

uint32_t test_helpers_pass_count(void)
{
    return g_pass_count;
}

uint32_t test_helpers_fail_count(void)
{
    return g_fail_count;
}

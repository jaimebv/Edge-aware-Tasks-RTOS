/*
 * Port-board regression test harness.
 *
 * This file exercises the EEAA board abstraction through hardware-backed
 * invariants rather than brittle exact values. The goal is to validate that
 * the board layer reports sane core identity and timing behavior on the active
 * target.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

#include "port/port_board.h"
#include "port/port_rtos.h"
#include "test_helpers.h"

#define TEST_CORE_COUNT_MAX        32U
#define TEST_CYCLE_SAMPLE_COUNT     4U
#define TEST_CYCLE_DELAY_MS        10U
#define TEST_TIME_DELAY_MS        100U
#define TEST_CONVERSION_SAMPLE_MS    7U
#define TEST_ALLOWED_MS_MIN        80U
#define TEST_ALLOWED_MS_MAX       250U

static bool capture_cycle_samples(uint32_t *samples, size_t count)
{
    size_t i;

    if (samples == NULL || count == 0U) {
        return false;
    }

    samples[0] = eaPort_Get_Cpu_Cycles();
    for (i = 1U; i < count; ++i) {
        eaPort_Delay_Milliseconds(TEST_CYCLE_DELAY_MS);
        samples[i] = eaPort_Get_Cpu_Cycles();
        if (samples[i] < samples[i - 1U]) {
            return false;
        }
    }

    return true;
}

static void test_core_identity(void)
{
    uint8_t core_id = eaPort_Get_Core_id();
    uint8_t num_cores = eaPort_Get_Num_Cores();

    expect_true("core count positive", num_cores >= 1U, "core count must be at least one");
    expect_true("core count reasonable", num_cores <= TEST_CORE_COUNT_MAX, "core count is unexpectedly large");
    expect_true("core id in range", core_id < num_cores, "core id must be less than the reported core count");
    pass("board core identity");
}

static void test_cpu_timing_relationship(void)
{
    uint32_t freq_mhz = eaPort_Get_Cpu_Freq();
    uint32_t cycles_per_ms = eaPort_Get_Cpu_Cycles_per_ms();

    expect_true("cpu frequency readable", freq_mhz > 0U, "cpu frequency must be positive");
    expect_true("cycles per ms readable", cycles_per_ms > 0U, "cycles per ms must be positive");
    expect_true("cycles per ms relation", cycles_per_ms == (freq_mhz * 1000U), "cycles per ms must match frequency * 1000");
    pass("board timing configuration");
}

static void test_cycle_counter_monotonicity(void)
{
    uint32_t samples[TEST_CYCLE_SAMPLE_COUNT] = {0};
    size_t i;

    expect_true("cycle samples monotonic", capture_cycle_samples(samples, TEST_CYCLE_SAMPLE_COUNT), "cycle counter must not move backwards over a short window");

    for (i = 1U; i < TEST_CYCLE_SAMPLE_COUNT; ++i) {
        expect_true("cycle sample progression", samples[i] >= samples[i - 1U], "cycle samples must be non-decreasing");
    }

    expect_true("cycle samples advanced", samples[TEST_CYCLE_SAMPLE_COUNT - 1U] > samples[0], "cycle counter should advance over time");
    pass("board cycle monotonicity");
}

static void test_cycles_to_ms_consistency(void)
{
    uint32_t cycles_per_ms = eaPort_Get_Cpu_Cycles_per_ms();
    uint32_t exact_cycles = cycles_per_ms * TEST_CONVERSION_SAMPLE_MS;
    uint32_t exact_ms = eaPort_Cycles_to_ms(exact_cycles);
    uint32_t start_cycles;
    uint32_t end_cycles;
    uint32_t delta_cycles;
    uint32_t measured_ms;

    expect_true("exact cycle conversion", exact_ms == TEST_CONVERSION_SAMPLE_MS, "cycles_to_ms must convert exact cycle multiples consistently");

    start_cycles = eaPort_Get_Cpu_Cycles();
    eaPort_Delay_Milliseconds(TEST_TIME_DELAY_MS);
    end_cycles = eaPort_Get_Cpu_Cycles();
    delta_cycles = (end_cycles >= start_cycles)
        ? (end_cycles - start_cycles)
        : ((UINT32_MAX - start_cycles) + end_cycles + 1U);
    measured_ms = eaPort_Cycles_to_ms(delta_cycles);

    expect_true("measured delta nonzero", measured_ms > 0U, "measured delta should convert to a non-zero duration");
    expect_true("measured delta reasonable", measured_ms >= TEST_ALLOWED_MS_MIN && measured_ms <= TEST_ALLOWED_MS_MAX,
                "measured delta should be in a reasonable range for a 100ms delay");
    pass("board cycle-to-ms consistency");
}

void test_port_board_run(void)
{
    printf("=== Port-board test harness ===\n");
    test_helpers_reset_counts();

    test_core_identity();
    test_cpu_timing_relationship();
    test_cycle_counter_monotonicity();
    test_cycles_to_ms_consistency();

    printf("=== Port-board tests done: passes=%" PRIu32 " fails=%" PRIu32 " ===\n",
           test_helpers_pass_count(),
           test_helpers_fail_count());

    while (1) {
        eaPort_Delay_Milliseconds(5000U);
    }
}

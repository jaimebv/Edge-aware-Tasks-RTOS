/*
 * Shared demo entry points for the board application.
 *
 * The PlatformIO build selects one of these demos at compile time so we can
 * flash the happy-path and advanced samples independently without changing the
 * source tree between uploads.
 */

#ifndef DEMO_EXAMPLES_H
#define DEMO_EXAMPLES_H

#ifdef __cplusplus
extern "C" {
#endif

void run_happy_path_example(void);
void run_advanced_api_example(void);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_EXAMPLES_H */

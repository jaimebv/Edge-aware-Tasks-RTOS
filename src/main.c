#include "examples/demo_examples.h"

void app_main(void)
{
#if defined(EA_EXAMPLE_HAPPY_PATH)
    run_happy_path_example();
#elif defined(EA_EXAMPLE_HELLO_WORLD)
    run_hello_world_example();
#elif defined(EA_EXAMPLE_ADVANCED_API)
    run_advanced_api_example();
#else
    run_advanced_api_example();
#endif
}

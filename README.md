# Edge-aware-Tasks-RTOS

ESP-IDF / PlatformIO project for experimenting with edge-aware task management.

The current demo in `src/main.c` creates a paired client/server task flow using the
`edge_task_pair_spec_t` (creation-time queue sizing) and `edge_task_pair_runtime_t`
(shared queue/handle state).

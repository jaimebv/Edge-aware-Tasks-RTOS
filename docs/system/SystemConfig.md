# System Configuration

This page explains the configuration constants in `components/EEAA/include/config/config_ea_system.h` and how they affect the system.

## 1. Configuration model

The configuration header is intentionally small. It defines the compile-time switches that shape the platform selection, scheduler selection, and basic resource limits used by the EEAA component.

These values are not runtime preferences; they define the build identity of the firmware.

## 2. Platform selection

### `CONFIG_EA_ESP32_BOARD`

```c
#define CONFIG_EA_ESP32_BOARD  0
```

This is the platform identifier for the ESP32 target.
It is used to make the build self-describing and to support conditional compilation if more board families are added later.

### `CONFIG_EA_PLATFORM`

```c
#define CONFIG_EA_PLATFORM CONFIG_EA_ESP32_BOARD
```

This selects the active platform family for the build.
In the current repository, the system is fixed to the ESP32 board profile.

## 3. Scheduler selection

### `CONFIG_EA_FREERTOS_SCHEDULER`

```c
#define CONFIG_EA_FREERTOS_SCHEDULER  0
```

This identifies the FreeRTOS scheduler backend.
The codebase uses FreeRTOS on ESP32, so this is the active scheduler family.

### `CONFIG_EA_CUSTOM_SCHEDULER`

```c
#define CONFIG_EA_CUSTOM_SCHEDULER    1
```

This marks the custom scheduling mode identifier used by the component's configuration model.
Even when the code runs on FreeRTOS, this macro exists to allow the project to distinguish scheduler policy from the underlying RTOS backend.

### `CONFIG_EA_RTOS_SCHEDULER`

```c
#define CONFIG_EA_RTOS_SCHEDULER CONFIG_EA_FREERTOS_SCHEDULER
```

This selects the active RTOS scheduler identifier.
At present, the firmware is built on the FreeRTOS scheduler path.

## 4. Debug configuration

### `CONFIG_DEBUG_FLAG`

```c
#define CONFIG_DEBUG_FLAG
```

This macro enables debug logging throughout the EEAA components.
It is intentionally lightweight: the code checks for the presence of the macro rather than a numeric level.

Practical effect:

- it allows debug prints in the component code
- it is useful during integration and hardware validation
- it should not be treated as a complete logging framework

If the project later needs structured log levels, this macro can evolve into a richer build-time logging policy.

## 5. Resource limits

### `CONFIG_EA_MAX_TASK_NAME_LEN`

```c
#define CONFIG_EA_MAX_TASK_NAME_LEN  16
```

This is the maximum length of a task name stored by the EEAA task manager.
It affects:

- runtime names
- snapshot names
- host labels
- internal metadata buffers

The limit is important because task names are copied into fixed-size arrays.
Keeping the size bounded avoids uncontrolled heap use and makes the system deterministic.

### `CONFIG_EA_MAX_TASKS`

```c
#define CONFIG_EA_MAX_TASKS  16
```

This is the maximum number of active monitored tasks.
It bounds the size of the monitoring arrays and therefore the size of the task manager's monitoring store.

This limit matters for three reasons:

1. **Memory predictability**
   - the monitoring store is static and bounded

2. **Regression testing**
   - the tests intentionally exercise growth, reuse, and cleanup within this ceiling

3. **Operational clarity**
   - the framework should fail cleanly when the monitored-task ceiling is reached

## 6. How the configuration affects behavior

### Task naming

Task name lengths are constrained by `CONFIG_EA_MAX_TASK_NAME_LEN`.
This affects both user-visible names and internal helper-generated names like:

- `TaskName-cl-0`
- `TaskName-sv-0`
- `TaskName-lc-0`

### Monitoring capacity

The monitoring arrays are sized by `CONFIG_EA_MAX_TASKS`.
The task manager uses this value to decide how many active task records can exist at once.

### Platform behavior

The platform and scheduler macros make the build identity explicit.
This is especially useful when adding a future port or when separating shared code from board-specific code.

## 7. Configuration design philosophy

The config header is small on purpose.

It should remain a place for:

- board identity
- scheduler identity
- resource ceilings
- debug toggles

It should **not** become a dumping ground for application logic or ad hoc test settings.

If a new requirement arises, prefer one of these paths:

- a documented new configuration macro in the same header
- a port-specific header
- a build-system option
- a module-specific constant when the value is local to one component

## 8. Practical guidance for contributors

When you change configuration values:

- update the code that depends on them
- update the task manager and port docs if the behavior changes
- update tests if the resource limit or platform identity affects runtime behavior
- update Doxygen comments in the config header

## 9. Doxygen reference

The exact declarations are documented in:

- `components/EEAA/include/config/config_ea_system.h`
- the generated Doxygen API reference

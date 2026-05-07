
## How to Add a New Board

If you want to run this RTOS on a new board (e.g., an STM32 or a RISC-V processor), follow these steps:

### 1. Register the Board
Open `include/config/config_ea_system.h` and add a new unique ID for your board:
```c
#define CONFIG_EA_MY_NEW_BOARD  2  // Use a unique number
```

### 2. Create the Implementation
Create a new C file in `src/port/` named `port_board_mynewboard.c`. You must implement the functions defined in `include/port/port_board_esp32.h` (or similar board headers). Key functions include:
- `eaPort_Get_Core_id()`: Which CPU core am I on?
- `eaPort_Get_Cpu_Cycles()`: Read the high-speed cycle counter.
- `eaPort_Cycles_to_ms()`: Convert those cycles into milliseconds.

### 3. Update the Selector
Open `include/port/port_board.h` and add a conditional include:
```c
#if CONFIG_EA_PLATFORM == CONFIG_EA_MY_NEW_BOARD
    #include "port_board_mynewboard.h"
#endif
```

---

## How to Add a New RTOS

If you want to use a different scheduler (e.g., Zephyr, RTX, or a custom one):

### 1. Register the RTOS
Open `include/config/config_ea_system.h`:
```c
#define CONFIG_EA_MY_NEW_RTOS  2 
```

### 2. Create the Type Mappings
Verify `include/port/port_interface_types.h`. This file defines abstract types like `eaPort_task_t`. Your new RTOS must be able to map its internal handles to these `void*` pointers.

### 3. Create the Implementation
Create `src/port/port_rtos_mynewrtos.c` and implement the functions defined in `include/port/port_rtos.h`:
- `eaPort_Task_Create_Pinned_to_Core()`: Wrap your RTOS's task creation.
- `eaPort_Mutex_Create()`: Wrap your RTOS's mutex creation.
- `eaPort_Delay_Milliseconds()`: Wrap your RTOS's delay function.

### 4. Update the Selector
Open `include/port/port_rtos.h` and include your new header:
```c
#if CONFIG_EA_RTOS_SCHEDULER == CONFIG_EA_MY_NEW_RTOS
    #include "port_rtos_mynewrtos.h"
#endif
```


## Folder Structure Reference

- `include/port/`: Where "What it does" is defined (Header files).
- `src/port/`: Where "How it does it" is implemented (Source files).
- `examples/port/`: Look here for code snippets on how to use these APIs!

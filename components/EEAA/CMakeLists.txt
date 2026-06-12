# cmake_minimum_required(VERSION 3.5)

# Register this folder as an ESP-IDF component:
idf_component_register(
  SRCS  "src/port/port_board_esp32.c"
        "src/port/port_rtos_freertos.c"
        "src/core/task_manager.c"
        "src/core/offloader.c"
        "src/core/offloader_policy_simple.c"
  INCLUDE_DIRS "include"
  REQUIRES    driver
              esp_timer
              esp_system
              freertos
              esp_hw_support
              log
              esp_wifi
              esp_netif
              esp_event 
              esp_http_client
              nvs_flash
              lwip
)

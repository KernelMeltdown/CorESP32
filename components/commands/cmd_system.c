/**
 * System Commands
 */

#include "command_registry.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <inttypes.h>

// heap command
static esp_err_t cmd_heap_execute(int argc, char** argv) {
    printf("\n");
    printf("Memory Status:\n");
    printf("---------------\n");
    printf("Free Heap:     %" PRIu32 " bytes\n", esp_get_free_heap_size());
    printf("Min Free Heap: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
    printf("\n");
    return ESP_OK;
}

// restart command
static esp_err_t cmd_restart_execute(int argc, char** argv) {
    printf("Restarting system...\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
}

// version command
static esp_err_t cmd_version_execute(int argc, char** argv) {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    printf("\n");
    printf("CorESP32 v7.0\n");
    printf("-------------\n");
    printf("Chip:     ESP32-C6 (rev %d)\n", chip_info.revision);
    printf("Cores:    %d\n", chip_info.cores);
    printf("Features: WiFi%s%s\n",
           (chip_info.features & CHIP_FEATURE_BT) ? ", BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? ", BLE" : "");
    printf("IDF:      %s\n", esp_get_idf_version());
    printf("\n");
    return ESP_OK;
}

// Command definitions
static const command_t cmd_heap = {
    .name = "heap",
    .description = "Show memory status",
    .usage = "heap",
    .execute = cmd_heap_execute
};

static const command_t cmd_restart = {
    .name = "restart",
    .description = "Restart system",
    .usage = "restart",
    .execute = cmd_restart_execute
};

static const command_t cmd_version = {
    .name = "version",
    .description = "Show system version",
    .usage = "version",
    .execute = cmd_version_execute
};

// Register all system commands
void register_system_commands(void) {
    command_register(&cmd_heap);
    command_register(&cmd_restart);
    command_register(&cmd_version);
    // tasks command removed (vTaskList not available)
}
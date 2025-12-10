/**
 * CorESP32 v7.0 - Main Application Entry (Dynamic Config)
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "littlefs_mount.h"
#include "config_loader.h"
#include "hw_adapter.h"
#include "coreshell.h"
#include "command_registry.h"

static const char* TAG = "APP_MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  CorESP32 v7.0");
    ESP_LOGI(TAG, "  Application Starting");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    
    // Mount storage
    ESP_LOGI(TAG, "Mounting Storage...");
    esp_err_t ret = storage_mount_littlefs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount storage!");
    }
    ESP_LOGI(TAG, "");
    
    // Load configuration (dynamic allocation)
    ESP_LOGI(TAG, "Loading Configuration...");
    ret = config_load("/littlefs/config/system.json");
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Config load failed, defaults loaded");
    }
    
    // Check if config loaded
    if (!g_boot_config) {
        ESP_LOGE(TAG, "FATAL: Config not loaded!");
        vTaskDelay(portMAX_DELAY);
        return;
    }
    
    // Print config
    config_print_summary();
    ESP_LOGI(TAG, "");
    
    // Initialize hardware adapter
    ESP_LOGI(TAG, "Initializing Hardware Adapter...");
    hw_adapter_init(g_boot_config);
    
    // Auto-init if enabled
    if (g_boot_config->config_mode == CONFIG_MODE_AUTO_INIT) {
        ESP_LOGI(TAG, "Auto-Init Mode: Initializing hardware from config...");
        hw_adapter_auto_init(g_boot_config);
    } else {
        ESP_LOGI(TAG, "Minimal Mode: Hardware init via commands only");
    }
    ESP_LOGI(TAG, "");
    
    // Initialize command registry
    ESP_LOGI(TAG, "Initializing Command Registry...");
    command_registry_init();
    
    // Register builtin commands
    ESP_LOGI(TAG, "Registering Builtin Commands...");
    register_builtin_commands();
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  System Ready");
    ESP_LOGI(TAG, "  Type 'help' for commands");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // Start shell
    coreshell_start();
    
    // Should never reach here
    ESP_LOGE(TAG, "Shell exited unexpectedly!");
    vTaskDelay(portMAX_DELAY);
}
/**
 * Config Commands - JSON Configuration Management
 */

#include "command_registry.h"
#include "config_loader.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char* TAG = "CMD_CONFIG";

// config:show
static esp_err_t cmd_config_show(void) {
    if (!g_boot_config) {
        printf("No config loaded\n");
        return ESP_FAIL;
    }
    
    printf("\n");
    printf("Current Configuration:\n");
    printf("=====================\n");
    printf("  Version:     %s\n", g_boot_config->version);
    printf("  Device:      %s\n", g_boot_config->device_name);
    printf("  Mode:        %s\n", 
           g_boot_config->config_mode == CONFIG_MODE_AUTO_INIT ? "AUTO_INIT" : "MINIMAL");
    printf("  Console:     UART%d @ %d baud\n",
           g_boot_config->uart.console.uart_num,
           g_boot_config->uart.console.baudrate);
    printf("\n");
    printf("Features:\n");
    printf("  Shell:       %s\n", g_boot_config->features.shell_enabled ? "ON" : "OFF");
    printf("  Logging:     %s\n", g_boot_config->features.logging_enabled ? "ON" : "OFF");
    printf("\n");
    
    if (g_boot_config->auto_init.enabled) {
        printf("Auto-Init:\n");
        printf("  Enabled:     YES\n");
        printf("  GPIO Pins:   %d configured\n", g_boot_config->auto_init.gpio_count);
        for (int i = 0; i < g_boot_config->auto_init.gpio_count; i++) {
            printf("    - GPIO%d: %s (%s)\n",
                   g_boot_config->auto_init.gpio[i].pin,
                   g_boot_config->auto_init.gpio[i].name,
                   g_boot_config->auto_init.gpio[i].mode == 1 ? "OUTPUT" : "INPUT");
        }
    } else {
        printf("Auto-Init:   DISABLED\n");
    }
    
    printf("\n");
    return ESP_OK;
}

// config:load
static esp_err_t cmd_config_load(const char* path) {
    printf("Loading config from: %s\n", path);
    
    esp_err_t ret = config_load(path);
    
    if (ret == ESP_OK) {
        printf("✓ Config loaded successfully\n");
        cmd_config_show();
    } else {
        printf("✗ Failed to load config: %s\n", esp_err_to_name(ret));
    }
    
    return ret;
}

// config:save
static esp_err_t cmd_config_save(const char* path) {
    if (!g_boot_config) {
        printf("No config to save\n");
        return ESP_FAIL;
    }
    
    printf("Saving config to: %s\n", path);
    
    esp_err_t ret = config_save(path);
    
    if (ret == ESP_OK) {
        printf("✓ Config saved successfully\n");
    } else {
        printf("✗ Failed to save config: %s\n", esp_err_to_name(ret));
    }
    
    return ret;
}

// config:reset
static esp_err_t cmd_config_reset(void) {
    printf("Resetting to default configuration...\n");
    
    esp_err_t ret = config_load_defaults();
    
    if (ret == ESP_OK) {
        printf("✓ Config reset to defaults\n");
        cmd_config_show();
    } else {
        printf("✗ Failed to reset config\n");
    }
    
    return ret;
}

// config:create-default
static esp_err_t cmd_config_create_default(void) {
    printf("Creating default config file...\n");
    
    // Load defaults
    config_load_defaults();
    
    // Save to file
    esp_err_t ret = config_save("/littlefs/config/system.json");
    
    if (ret == ESP_OK) {
        printf("✓ Default config created at /littlefs/config/system.json\n");
    } else {
        printf("✗ Failed to create config file\n");
    }
    
    return ret;
}

// Main command handler
static esp_err_t cmd_config_execute(int argc, char** argv) {
    if (argc < 2) {
        printf("\n");
        printf("Usage:\n");
        printf("  config:show                   - Show current config\n");
        printf("  config:load <file>            - Load config from file\n");
        printf("  config:save <file>            - Save config to file\n");
        printf("  config:reset                  - Reset to defaults\n");
        printf("  config:create-default         - Create default config file\n");
        printf("\n");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char* action = argv[1];
    
    if (strcmp(action, "show") == 0) {
        return cmd_config_show();
    }
    else if (strcmp(action, "load") == 0) {
        if (argc < 3) {
            printf("Missing filename\n");
            return ESP_ERR_INVALID_ARG;
        }
        return cmd_config_load(argv[2]);
    }
    else if (strcmp(action, "save") == 0) {
        if (argc < 3) {
            printf("Missing filename\n");
            return ESP_ERR_INVALID_ARG;
        }
        return cmd_config_save(argv[2]);
    }
    else if (strcmp(action, "reset") == 0) {
        return cmd_config_reset();
    }
    else if (strcmp(action, "create-default") == 0) {
        return cmd_config_create_default();
    }
    else {
        printf("Unknown action: %s\n", action);
        return ESP_ERR_INVALID_ARG;
    }
}

static const command_t cmd_config = {
    .name = "config",
    .description = "Configuration management (config:ACTION)",
    .usage = "config:show | config:load <file> | config:save <file>",
    .execute = cmd_config_execute
};

void register_config_commands(void) {
    command_register(&cmd_config);
}
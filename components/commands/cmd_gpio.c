/**
 * GPIO Commands
 */

#include "command_registry.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* TAG = "CMD_GPIO";

// State tracking (für toggle)
static uint8_t g_pin_states[32] = {0};

// Helper: Parse Pin Number
static int parse_pin(const char* str) {
    int pin = atoi(str);
    if (pin < 0 || pin > 30) {
        return -1;
    }
    return pin;
}

// gpio:PIN:mode MODE
static esp_err_t cmd_gpio_mode(int pin, const char* mode) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    if (strcmp(mode, "output") == 0) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    }
    else if (strcmp(mode, "input") == 0) {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    }
    else if (strcmp(mode, "input_pullup") == 0) {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    }
    else if (strcmp(mode, "input_pulldown") == 0) {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    }
    else {
        printf("Invalid mode: %s\n", mode);
        printf("Valid modes: output, input, input_pullup, input_pulldown\n");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        printf("GPIO%d mode set to: %s\n", pin, mode);
    }
    
    return ret;
}

// gpio:PIN:write VALUE
static esp_err_t cmd_gpio_write(int pin, int value) {
    if (value != 0 && value != 1) {
        printf("Invalid value: %d (use 0 or 1)\n", value);
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = gpio_set_level(pin, value);
    if (ret == ESP_OK) {
        g_pin_states[pin] = value;
        printf("GPIO%d = %d\n", pin, value);
    }
    
    return ret;
}

// gpio:PIN:toggle
static esp_err_t cmd_gpio_toggle(int pin) {
    uint8_t new_state = !g_pin_states[pin];
    esp_err_t ret = gpio_set_level(pin, new_state);
    
    if (ret == ESP_OK) {
        g_pin_states[pin] = new_state;
        printf("GPIO%d toggled to %d\n", pin, new_state);
    }
    
    return ret;
}

// gpio:PIN:read
static esp_err_t cmd_gpio_read(int pin) {
    int level = gpio_get_level(pin);
    printf("GPIO%d = %d\n", pin, level);
    return ESP_OK;
}

// gpio:PIN:info
static esp_err_t cmd_gpio_info(int pin) {
    printf("\n");
    printf("GPIO%d Information:\n", pin);
    printf("  Pin:       %d\n", pin);
    printf("  Valid:     %s\n", (pin >= 0 && pin <= 30) ? "Yes" : "No");
    
    // Try to read current state
    int level = gpio_get_level(pin);
    printf("  Level:     %d\n", level);
    printf("  Tracked:   %d\n", g_pin_states[pin]);
    printf("\n");
    
    return ESP_OK;
}

// Main GPIO Command Handler
static esp_err_t cmd_gpio_execute(int argc, char** argv) {
    // Format: gpio:PIN:ACTION [VALUE]
    // argv[0] = "gpio"
    // argv[1] = "PIN" (from parsing)
    // argv[2] = "ACTION"
    // argv[3] = "VALUE" (optional)
    
    if (argc < 3) {
        printf("\n");
        printf("Usage:\n");
        printf("  gpio:PIN:mode MODE       - Set pin mode\n");
        printf("  gpio:PIN:write VALUE     - Write pin (0/1)\n");
        printf("  gpio:PIN:toggle          - Toggle pin\n");
        printf("  gpio:PIN:read            - Read pin\n");
        printf("  gpio:PIN:info            - Pin information\n");
        printf("\n");
        printf("Modes: output, input, input_pullup, input_pulldown\n");
        printf("Example: gpio:8:mode output\n");
        printf("\n");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Parse PIN
    int pin = parse_pin(argv[1]);
    if (pin < 0) {
        printf("Invalid pin: %s (use 0-30)\n", argv[1]);
        return ESP_ERR_INVALID_ARG;
    }
    
    const char* action = argv[2];
    
    // Dispatch action
    if (strcmp(action, "mode") == 0) {
        if (argc < 4) {
            printf("Missing mode argument\n");
            return ESP_ERR_INVALID_ARG;
        }
        return cmd_gpio_mode(pin, argv[3]);
    }
    else if (strcmp(action, "write") == 0) {
        if (argc < 4) {
            printf("Missing value argument\n");
            return ESP_ERR_INVALID_ARG;
        }
        int value = atoi(argv[3]);
        return cmd_gpio_write(pin, value);
    }
    else if (strcmp(action, "toggle") == 0) {
        return cmd_gpio_toggle(pin);
    }
    else if (strcmp(action, "read") == 0) {
        return cmd_gpio_read(pin);
    }
    else if (strcmp(action, "info") == 0) {
        return cmd_gpio_info(pin);
    }
    else {
        printf("Unknown action: %s\n", action);
        return ESP_ERR_INVALID_ARG;
    }
}

static const command_t cmd_gpio = {
    .name = "gpio",
    .description = "GPIO control (gpio:PIN:ACTION)",
    .usage = "gpio:PIN:mode output | gpio:PIN:write 1 | gpio:PIN:read",
    .execute = cmd_gpio_execute
};

void register_gpio_commands(void) {
    command_register(&cmd_gpio);
}
/**
 * Hardware Adapter Implementation
 */

#include "hw_adapter.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char* TAG = "HW_ADAPTER";
static config_t* g_config = NULL;

esp_err_t hw_adapter_init(config_t* config) {
    ESP_LOGI(TAG, "Hardware Adapter initialized");
    g_config = config;
    return ESP_OK;
}

esp_err_t hw_adapter_auto_init(config_t* config) {
    if (!config->auto_init.enabled) {
        ESP_LOGI(TAG, "Auto-init disabled");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Auto-initializing hardware...");
    
    // Init GPIO pins
    for (int i = 0; i < config->auto_init.gpio_count; i++) {
        gpio_pin_config_t* pin = &config->auto_init.gpio[i];
        
        ESP_LOGI(TAG, "  GPIO%d: %s (%s)", 
                 pin->pin, 
                 pin->mode == GPIO_MODE_OUTPUT ? "OUTPUT" : "INPUT",
                 pin->name);
        
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pin->pin),
            .mode = pin->mode,
            .pull_up_en = (pin->pull == GPIO_PULLUP_ENABLE) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
            .pull_down_en = (pin->pull == GPIO_PULLDOWN_ENABLE) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
        
        // Set initial value for outputs
        if (pin->mode == GPIO_MODE_OUTPUT) {
            gpio_set_level(pin->pin, pin->initial);
        }
    }
    
    // Init SPI
    if (config->auto_init.spi.enabled) {
        ESP_LOGI(TAG, "  SPI: CLK=%d, MOSI=%d, MISO=%d, CS=%d",
                 config->auto_init.spi.clk_pin,
                 config->auto_init.spi.mosi_pin,
                 config->auto_init.spi.miso_pin,
                 config->auto_init.spi.cs_pin);
        // SPI init code here (later)
    }
    
    // Init I2C
    if (config->auto_init.i2c.enabled) {
        ESP_LOGI(TAG, "  I2C: SCL=%d, SDA=%d",
                 config->auto_init.i2c.scl_pin,
                 config->auto_init.i2c.sda_pin);
        // I2C init code here (later)
    }
    
    ESP_LOGI(TAG, "✓ Auto-init complete");
    return ESP_OK;
}
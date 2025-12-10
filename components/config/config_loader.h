/**
 * Config Component - JSON Configuration Loader
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Config Modes
    typedef enum
    {
        CONFIG_MODE_MINIMAL = 0,
        CONFIG_MODE_AUTO_INIT = 1,
    } config_mode_t;

    // UART Config
    typedef struct
    {
        int uart_num;
        int tx_pin;
        int rx_pin;
        int baudrate;
    } uart_config_data_t;

    // GPIO Pin Config (for auto-init)
    typedef struct
    {
        int pin;
        int mode;
        int initial;
        int pull;
        char name[32];
    } gpio_pin_config_t;

    // SPI Config
    typedef struct
    {
        bool enabled;
        int clk_pin;
        int mosi_pin;
        int miso_pin;
        int cs_pin;
        int speed_hz;
    } spi_config_data_t;

    // I2C Config
    typedef struct
    {
        bool enabled;
        int scl_pin;
        int sda_pin;
        int freq_hz;
    } i2c_config_data_t;

    // Auto-Init Config
    typedef struct
    {
        bool enabled;
        gpio_pin_config_t gpio[4]; // Max 4 auto-init pins (reduced for memory)
        int gpio_count;
        spi_config_data_t spi;
        i2c_config_data_t i2c;
    } auto_init_config_t;

    // Aliases
    typedef struct
    {
        int led;
        int button;
    } aliases_config_t;

    // Features
    typedef struct
    {
        bool shell_enabled;
        bool logging_enabled;
        bool command_history;
    } features_config_t;

    // Main Config Structure
    typedef struct
    {
        char version[16];
        char device_name[64];
        config_mode_t config_mode;

        struct
        {
            uart_config_data_t console;
        } uart;

        auto_init_config_t auto_init;
        aliases_config_t aliases;
        features_config_t features;

        bool loaded;
    } config_t;

    // Global config pointer (dynamically allocated)
    extern config_t *g_boot_config;

    /**
     * @brief Load configuration from JSON file
     *
     * @param path Path to JSON file
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t config_load(const char *path);

    /**
     * @brief Load default configuration
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t config_load_defaults(void);

    /**
     * @brief Saves default configuration
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t config_save(const char *path);

    /**
     * @brief Free configuration memory
     */
    void config_free(void);

    /**
     * @brief Print configuration summary
     */
    void config_print_summary(void);

#ifdef __cplusplus
}
#endif
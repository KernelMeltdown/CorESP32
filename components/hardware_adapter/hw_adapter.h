/**
 * Hardware Adapter - Zero-Hardcoding Hardware Abstraction
 */
#pragma once

#include "esp_err.h"
#include "config_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize hardware adapter
 * 
 * @param config Pointer to system config
 * @return ESP_OK on success
 */
esp_err_t hw_adapter_init(config_t* config);

/**
 * @brief Auto-initialize hardware from config (if enabled)
 * 
 * @param config Pointer to system config
 * @return ESP_OK on success
 */
esp_err_t hw_adapter_auto_init(config_t* config);

#ifdef __cplusplus
}
#endif
/**
 * Storage Component - LittleFS Mount
 */
#pragma once

#include "esp_err.h"
#include "esp_littlefs.h"  // From joltwallet/littlefs

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mount LittleFS filesystem
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t storage_mount_littlefs(void);

/**
 * @brief Unmount LittleFS filesystem
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t storage_unmount_littlefs(void);

/**
 * @brief Get LittleFS info (total/used space)
 * 
 * @param total_bytes Pointer to store total bytes
 * @param used_bytes Pointer to store used bytes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t storage_get_info(size_t* total_bytes, size_t* used_bytes);

#ifdef __cplusplus
}
#endif
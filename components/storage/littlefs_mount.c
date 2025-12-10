/**
 * Storage Component - LittleFS Mount Implementation
 */

#include "littlefs_mount.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "STORAGE";
static bool g_mounted = false;

esp_err_t storage_mount_littlefs(void)
{
    if (g_mounted)
    {
        ESP_LOGW(TAG, "LittleFS already mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Mounting LittleFS...");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "storage", // ← CHANGE
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    // Get partition info
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get LittleFS info (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "  Partition: %s", conf.partition_label);
        ESP_LOGI(TAG, "  Total:     %d KB", total / 1024);
        ESP_LOGI(TAG, "  Used:      %d KB", used / 1024);
        ESP_LOGI(TAG, "  Free:      %d KB", (total - used) / 1024);
    }

    g_mounted = true;
    ESP_LOGI(TAG, "✓ LittleFS mounted at /littlefs");

    return ESP_OK;
}

esp_err_t storage_unmount_littlefs(void)
{
    if (!g_mounted)
    {
        ESP_LOGW(TAG, "LittleFS not mounted");
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_littlefs_unregister("littlefs");
    if (ret == ESP_OK)
    {
        g_mounted = false;
        ESP_LOGI(TAG, "LittleFS unmounted");
    }

    return ret;
}

esp_err_t storage_get_info(size_t *total_bytes, size_t *used_bytes)
{
    if (!g_mounted)
    {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_littlefs_info("littlefs", total_bytes, used_bytes);
}
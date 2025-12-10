/**
 * I2C Commands - Bus Scanner & Communication
 */

#include "command_registry.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* TAG = "CMD_I2C";

static i2c_master_bus_handle_t bus_handle = NULL;
static bool i2c_initialized = false;

#define I2C_MASTER_SCL_IO    22  // Default SCL
#define I2C_MASTER_SDA_IO    21  // Default SDA
#define I2C_MASTER_FREQ_HZ   100000  // 100 kHz

// Initialize I2C Master
static esp_err_t i2c_master_init(void) {
    if (i2c_initialized) {
        return ESP_OK;
    }
    
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C master init failed");
        return ret;
    }
    
    i2c_initialized = true;
    ESP_LOGI(TAG, "I2C initialized (SDA=%d, SCL=%d)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    
    return ESP_OK;
}

// i2c:scan - Scan I2C bus for devices
static esp_err_t cmd_i2c_scan(void) {
    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK) {
        printf("I2C init failed\n");
        return ret;
    }
    
    printf("\n");
    printf("Scanning I2C bus...\n");
    printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
    
    int found = 0;
    
    for (int addr = 0; addr < 128; addr++) {
        if (addr % 16 == 0) {
            printf("%02X: ", addr);
        }
        
        // Try to probe device
        i2c_master_dev_handle_t dev_handle;
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        };
        
        ret = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
        if (ret == ESP_OK) {
            uint8_t data;
            ret = i2c_master_receive(dev_handle, &data, 1, 100);
            
            if (ret == ESP_OK) {
                printf("%02X ", addr);
                found++;
            } else {
                printf("-- ");
            }
            
            i2c_master_bus_rm_device(dev_handle);
        } else {
            printf("-- ");
        }
        
        if ((addr + 1) % 16 == 0) {
            printf("\n");
        }
    }
    
    printf("\n");
    printf("Found %d device(s)\n", found);
    printf("\n");
    
    return ESP_OK;
}

// i2c:read ADDR COUNT - Read bytes from I2C device
static esp_err_t cmd_i2c_read(uint8_t addr, int count) {
    if (count < 1 || count > 32) {
        printf("Invalid count: %d (use 1-32)\n", count);
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK) return ret;
    
    i2c_master_dev_handle_t dev_handle;
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    
    ret = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
    if (ret != ESP_OK) {
        printf("Failed to add device 0x%02X\n", addr);
        return ret;
    }
    
    uint8_t data[32];
    ret = i2c_master_receive(dev_handle, data, count, 1000);
    
    if (ret == ESP_OK) {
        printf("Read from 0x%02X: ", addr);
        for (int i = 0; i < count; i++) {
            printf("%02X ", data[i]);
        }
        printf("\n");
    } else {
        printf("Read failed from 0x%02X\n", addr);
    }
    
    i2c_master_bus_rm_device(dev_handle);
    
    return ret;
}

// Main I2C Command Handler
static esp_err_t cmd_i2c_execute(int argc, char** argv) {
    if (argc < 2) {
        printf("\n");
        printf("Usage:\n");
        printf("  i2c:scan              - Scan I2C bus for devices\n");
        printf("  i2c:read ADDR COUNT   - Read bytes from device\n");
        printf("\n");
        printf("Example: i2c:scan\n");
        printf("         i2c:read 0x3C 1\n");
        printf("\n");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char* action = argv[1];
    
    if (strcmp(action, "scan") == 0) {
        return cmd_i2c_scan();
    }
    else if (strcmp(action, "read") == 0) {
        if (argc < 4) {
            printf("Missing address or count\n");
            return ESP_ERR_INVALID_ARG;
        }
        
        uint8_t addr = (uint8_t)strtol(argv[2], NULL, 16);
        int count = atoi(argv[3]);
        
        return cmd_i2c_read(addr, count);
    }
    else {
        printf("Unknown action: %s\n", action);
        return ESP_ERR_INVALID_ARG;
    }
}

static const command_t cmd_i2c = {
    .name = "i2c",
    .description = "I2C bus control (i2c:ACTION)",
    .usage = "i2c:scan | i2c:read 0x3C 1",
    .execute = cmd_i2c_execute
};

void register_i2c_commands(void) {
    command_register(&cmd_i2c);
}
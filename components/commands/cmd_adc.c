/**
 * ADC Commands - Analog Input
 */

#include "command_registry.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "CMD_ADC";

// ADC Handle (global, initialized on first use)
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool adc_initialized = false;

// Initialize ADC
static esp_err_t adc_init(void) {
    if (adc_initialized) {
        return ESP_OK;
    }

    // ADC1 Init
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC unit");
        return ret;
    }

    // Calibration - Platform-specific
    bool calibration_supported = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    // ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, etc.
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
    if (ret == ESP_OK) {
        calibration_supported = true;
        ESP_LOGI(TAG, "Calibration: Curve Fitting");
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    // ESP32 (original)
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc1_cali_handle);
    if (ret == ESP_OK) {
        calibration_supported = true;
        ESP_LOGI(TAG, "Calibration: Line Fitting");
    }
#endif

    if (!calibration_supported) {
        ESP_LOGW(TAG, "Calibration not available, using raw values");
        adc1_cali_handle = NULL;
    }

    adc_initialized = true;
    ESP_LOGI(TAG, "ADC initialized");
    return ESP_OK;
}

// Configure ADC channel
static esp_err_t adc_config_channel(adc_channel_t channel) {
    if (!adc_initialized) {
        esp_err_t ret = adc_init();
        if (ret != ESP_OK) return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12, // 0-3.3V range
    };
    return adc_oneshot_config_channel(adc1_handle, channel, &config);
}

// adc:CHANNEL:read - Read raw ADC value
static esp_err_t cmd_adc_read(int channel) {
    if (channel < 0 || channel > 6) {
        printf("Invalid ADC channel: %d (use 0-6)\n", channel);
        return ESP_ERR_INVALID_ARG;
    }

    // Config channel
    esp_err_t ret = adc_config_channel((adc_channel_t)channel);
    if (ret != ESP_OK) {
        printf("Failed to configure ADC channel %d\n", channel);
        return ret;
    }

    // Read raw value
    int raw_value;
    ret = adc_oneshot_read(adc1_handle, (adc_channel_t)channel, &raw_value);
    if (ret != ESP_OK) {
        printf("Failed to read ADC channel %d\n", channel);
        return ret;
    }

    printf("ADC%d raw = %d (0-4095)\n", channel, raw_value);
    return ESP_OK;
}

// adc:CHANNEL:voltage - Read voltage in mV
static esp_err_t cmd_adc_voltage(int channel) {
    if (channel < 0 || channel > 6) {
        printf("Invalid ADC channel: %d (use 0-6)\n", channel);
        return ESP_ERR_INVALID_ARG;
    }

    // Config channel
    esp_err_t ret = adc_config_channel((adc_channel_t)channel);
    if (ret != ESP_OK) {
        printf("Failed to configure ADC channel %d\n", channel);
        return ret;
    }

    // Read raw value
    int raw_value;
    ret = adc_oneshot_read(adc1_handle, (adc_channel_t)channel, &raw_value);
    if (ret != ESP_OK) {
        printf("Failed to read ADC channel %d\n", channel);
        return ret;
    }

    // Convert to voltage
    if (adc1_cali_handle) {
        int voltage_mv;
        ret = adc_cali_raw_to_voltage(adc1_cali_handle, raw_value, &voltage_mv);
        if (ret == ESP_OK) {
            printf("ADC%d = %d mV (%.3f V)\n", channel, voltage_mv, voltage_mv / 1000.0f);
        } else {
            printf("Calibration failed, raw = %d\n", raw_value);
        }
    } else {
        // Estimate without calibration (rough)
        int voltage_mv = (raw_value * 3300) / 4095;
        printf("ADC%d ≈ %d mV (uncalibrated)\n", channel, voltage_mv);
    }

    return ESP_OK;
}

// adc:CHANNEL:info - Channel information
static esp_err_t cmd_adc_info(int channel) {
    printf("\n");
    printf("ADC Channel %d Information:\n", channel);
    printf("  Unit: ADC1\n");
    printf("  Bitwidth: 12-bit (0-4095)\n");
    printf("  Attenuation: 12dB (0-3.3V)\n");
    printf("  Calibrated: %s\n", adc1_cali_handle ? "Yes" : "No");
    printf("  Valid: %s\n", (channel >= 0 && channel <= 6) ? "Yes" : "No");
    printf("\n");
    printf("ESP32-C6 ADC1 Channels:\n");
    printf("  ADC1_CH0 = GPIO0\n");
    printf("  ADC1_CH1 = GPIO1\n");
    printf("  ADC1_CH2 = GPIO2\n");
    printf("  ADC1_CH3 = GPIO3\n");
    printf("  ADC1_CH4 = GPIO4\n");
    printf("  ADC1_CH5 = GPIO5\n");
    printf("  ADC1_CH6 = GPIO6\n");
    printf("\n");
    return ESP_OK;
}

// Main ADC Command Handler
static esp_err_t cmd_adc_execute(int argc, char** argv) {
    if (argc < 3) {
        printf("\n");
        printf("Usage:\n");
        printf("  adc:CHANNEL:read    - Read raw ADC value (0-4095)\n");
        printf("  adc:CHANNEL:voltage - Read voltage in mV\n");
        printf("  adc:CHANNEL:info    - Channel information\n");
        printf("\n");
        printf("Example: adc:0:voltage\n");
        printf("\n");
        return ESP_ERR_INVALID_ARG;
    }

    int channel = atoi(argv[1]);
    const char* action = argv[2];

    if (strcmp(action, "read") == 0) {
        return cmd_adc_read(channel);
    }
    else if (strcmp(action, "voltage") == 0) {
        return cmd_adc_voltage(channel);
    }
    else if (strcmp(action, "info") == 0) {
        return cmd_adc_info(channel);
    }
    else {
        printf("Unknown action: %s\n", action);
        return ESP_ERR_INVALID_ARG;
    }
}

static const command_t cmd_adc = {
    .name = "adc",
    .description = "ADC control (adc:CHANNEL:ACTION)",
    .usage = "adc:0:voltage | adc:0:read | adc:0:info",
    .execute = cmd_adc_execute
};

void register_adc_commands(void) {
    command_register(&cmd_adc);
}

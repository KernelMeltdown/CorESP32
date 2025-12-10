/**
 * PWM Commands - LED Control (LEDC)
 */

#include "command_registry.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

static const char* TAG = "CMD_PWM";

// PWM Configuration
static bool pwm_initialized = false;

// Initialize PWM (LEDC)
static esp_err_t pwm_init(void) {
    if (pwm_initialized) {
        return ESP_OK;
    }

    // Timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .freq_hz          = 5000,  // 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer");
        return ret;
    }

    pwm_initialized = true;
    ESP_LOGI(TAG, "PWM initialized");
    return ESP_OK;
}

// Configure PWM channel
static esp_err_t pwm_config_channel(int gpio_num, int channel) {
    if (!pwm_initialized) {
        esp_err_t ret = pwm_init();
        if (ret != ESP_OK) return ret;
    }

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = (ledc_channel_t)channel,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = gpio_num,
        .duty           = 0,
        .hpoint         = 0
    };
    return ledc_channel_config(&ledc_channel);
}

// pwm:GPIO:CHANNEL:duty:DUTY - Set PWM duty cycle (0-100%)
static esp_err_t cmd_pwm_duty(int gpio_num, int channel, int duty_percent) {
    if (duty_percent < 0 || duty_percent > 100) {
        printf("Invalid duty cycle: %d%% (use 0-100)\n", duty_percent);
        return ESP_ERR_INVALID_ARG;
    }

    // Configure channel if not already
    esp_err_t ret = pwm_config_channel(gpio_num, channel);
    if (ret != ESP_OK) {
        printf("Failed to configure PWM channel\n");
        return ret;
    }

    // Set duty cycle (13-bit resolution = 8192 levels)
    uint32_t duty = (duty_percent * 8191) / 100;
    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    if (ret != ESP_OK) {
        printf("Failed to set duty cycle\n");
        return ret;
    }

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    if (ret != ESP_OK) {
        printf("Failed to update duty cycle\n");
        return ret;
    }

    printf("PWM GPIO%d CH%d = %d%% duty\n", gpio_num, channel, duty_percent);
    return ESP_OK;
}

// pwm:GPIO:CHANNEL:freq:FREQUENCY - Set PWM frequency
static esp_err_t cmd_pwm_freq(int gpio_num, int channel, uint32_t frequency) {
    if (frequency < 1 || frequency > 40000000) {
        printf("Invalid frequency: %" PRIu32 " Hz (use 1-40000000)\n", frequency);
        return ESP_ERR_INVALID_ARG;
    }

    // Configure channel if not already
    esp_err_t ret = pwm_config_channel(gpio_num, channel);
    if (ret != ESP_OK) {
        printf("Failed to configure PWM channel\n");
        return ret;
    }

    // Set frequency
    ret = ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, frequency);
    if (ret != ESP_OK) {
        printf("Failed to set frequency\n");
        return ret;
    }

    printf("PWM frequency set to %" PRIu32 " Hz\n", frequency);
    return ESP_OK;
}

// pwm:GPIO:CHANNEL:stop - Stop PWM output
static esp_err_t cmd_pwm_stop(int gpio_num, int channel) {
    esp_err_t ret = ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
    if (ret == ESP_OK) {
        printf("PWM GPIO%d CH%d stopped\n", gpio_num, channel);
    } else {
        printf("Failed to stop PWM\n");
    }
    return ret;
}

// pwm:GPIO:CHANNEL:info - Show PWM info
static esp_err_t cmd_pwm_info(int gpio_num, int channel) {
    printf("\n");
    printf("PWM Channel %d Information:\n", channel);
    printf("  GPIO: %d\n", gpio_num);
    printf("  Timer: LEDC_TIMER_0\n");
    printf("  Speed Mode: Low Speed\n");
    printf("  Resolution: 13-bit (0-8191)\n");
    printf("  Frequency: Variable (1 Hz - 40 MHz)\n");
    printf("\n");
    return ESP_OK;
}

// Main PWM Command Handler
static esp_err_t cmd_pwm_execute(int argc, char** argv) {
    if (argc < 4) {
        printf("\n");
        printf("Usage:\n");
        printf("  pwm:GPIO:CH:duty:PERCENT   - Set duty cycle (0-100%%)\n");
        printf("  pwm:GPIO:CH:freq:HZ        - Set frequency\n");
        printf("  pwm:GPIO:CH:stop           - Stop PWM\n");
        printf("  pwm:GPIO:CH:info           - Show info\n");
        printf("\n");
        printf("Example: pwm:8:0:duty:50\n");
        printf("\n");
        return ESP_ERR_INVALID_ARG;
    }

    int gpio_num = atoi(argv[1]);
    int channel = atoi(argv[2]);
    const char* action = argv[3];

    if (strcmp(action, "duty") == 0 && argc >= 5) {
        int duty = atoi(argv[4]);
        return cmd_pwm_duty(gpio_num, channel, duty);
    }
    else if (strcmp(action, "freq") == 0 && argc >= 5) {
        uint32_t freq = (uint32_t)atoi(argv[4]);
        return cmd_pwm_freq(gpio_num, channel, freq);
    }
    else if (strcmp(action, "stop") == 0) {
        return cmd_pwm_stop(gpio_num, channel);
    }
    else if (strcmp(action, "info") == 0) {
        return cmd_pwm_info(gpio_num, channel);
    }
    else {
        printf("Unknown action: %s\n", action);
        return ESP_ERR_INVALID_ARG;
    }
}

static const command_t cmd_pwm = {
    .name = "pwm",
    .description = "PWM control (pwm:GPIO:CHANNEL:ACTION)",
    .usage = "pwm:8:0:duty:50 | pwm:8:0:freq:1000",
    .execute = cmd_pwm_execute
};

void register_pwm_commands(void) {
    command_register(&cmd_pwm);
}

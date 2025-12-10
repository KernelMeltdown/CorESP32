/**
 * Config Loader Implementation
 */

#include "config_loader.h"
#include "jsmn.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "CONFIG";

config_t *g_boot_config = NULL;

// Helper: JSON string compare
static int json_str_eq(const char *json, jsmntok_t *tok, const char *s)
{
    return (tok->type == JSMN_STRING &&
            (int)strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, tok->end - tok->start) == 0);
}

// Helper: JSON string copy
static void json_str_copy(char *dst, size_t dst_size, const char *json, jsmntok_t *tok)
{
    int len = tok->end - tok->start;
    if (len >= (int)dst_size)
        len = dst_size - 1;
    strncpy(dst, json + tok->start, len);
    dst[len] = '\0';
}

// Helper: JSON int
static int json_int(const char *json, jsmntok_t *tok)
{
    return atoi(json + tok->start);
}

// Helper: JSON bool
static bool json_bool(const char *json, jsmntok_t *tok)
{
    return (json[tok->start] == 't');
}

esp_err_t config_load_defaults(void)
{
    ESP_LOGI(TAG, "Loading default configuration...");

    if (g_boot_config)
    {
        free(g_boot_config);
    }

    g_boot_config = calloc(1, sizeof(config_t));
    if (!g_boot_config)
    {
        ESP_LOGE(TAG, "Failed to allocate config!");
        return ESP_ERR_NO_MEM;
    }

    strcpy(g_boot_config->version, "7.0");
    strcpy(g_boot_config->device_name, "CorESP32");
    g_boot_config->config_mode = CONFIG_MODE_MINIMAL;

    g_boot_config->uart.console.uart_num = 0;
    g_boot_config->uart.console.baudrate = 115200;

    g_boot_config->features.shell_enabled = true;
    g_boot_config->features.logging_enabled = true;

    g_boot_config->loaded = true;

    ESP_LOGI(TAG, "✓ Default config loaded");
    return ESP_OK;
}

esp_err_t config_load(const char *path)
{
    ESP_LOGI(TAG, "Loading config from %s...", path);

    FILE *f = fopen(path, "r");
    if (!f)
    {
        ESP_LOGW(TAG, "Config file not found, using defaults");
        return config_load_defaults();
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 8192)
    {
        fclose(f);
        ESP_LOGE(TAG, "Invalid file size: %ld", fsize);
        return config_load_defaults();
    }

    // Allocate on HEAP instead of stack
    char *json = malloc(fsize + 1);
    if (!json)
    {
        fclose(f);
        ESP_LOGE(TAG, "Failed to allocate JSON buffer");
        return ESP_ERR_NO_MEM;
    }

    fread(json, 1, fsize, f);
    json[fsize] = '\0';
    fclose(f);

    // Parse JSON (also on heap)
    jsmn_parser parser;
    jsmntok_t *tokens = malloc(128 * sizeof(jsmntok_t)); // ← HEAP
    if (!tokens)
    {
        free(json);
        ESP_LOGE(TAG, "Failed to allocate tokens");
        return ESP_ERR_NO_MEM;
    }

    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, json, fsize, tokens, 128);

    if (token_count < 0)
    {
        ESP_LOGE(TAG, "JSON parse error");
        free(tokens);
        free(json);
        return config_load_defaults();
    }

    // Allocate config
    if (g_boot_config)
    {
        free(g_boot_config);
    }

    g_boot_config = calloc(1, sizeof(config_t));
    if (!g_boot_config)
    {
        free(tokens);
        free(json);
        return ESP_ERR_NO_MEM;
    }

    // Load defaults first
    strcpy(g_boot_config->version, "7.0");
    strcpy(g_boot_config->device_name, "CorESP32");
    g_boot_config->config_mode = CONFIG_MODE_MINIMAL;
    g_boot_config->features.shell_enabled = true;
    g_boot_config->features.logging_enabled = true;
    g_boot_config->uart.console.uart_num = 0;
    g_boot_config->uart.console.baudrate = 115200;

    // Parse root object (simplified - nur basics)
    for (int i = 1; i < token_count && i < 50; i++)
    {
        jsmntok_t *key = &tokens[i];
        if (key->type != JSMN_STRING)
            continue;

        jsmntok_t *val = &tokens[i + 1];

        if (json_str_eq(json, key, "device_name"))
        {
            json_str_copy(g_boot_config->device_name,
                          sizeof(g_boot_config->device_name), json, val);
        }
        else if (json_str_eq(json, key, "config_mode"))
        {
            char mode[16];
            json_str_copy(mode, sizeof(mode), json, val);
            if (strcmp(mode, "auto_init") == 0)
            {
                g_boot_config->config_mode = CONFIG_MODE_AUTO_INIT;
            }
        }
    }

    g_boot_config->loaded = true;

    // Cleanup
    free(tokens);
    free(json);

    ESP_LOGI(TAG, "✓ Config loaded from file");
    return ESP_OK;
}

esp_err_t config_save(const char *path)
{
    if (!g_boot_config)
    {
        ESP_LOGE(TAG, "No config to save");
        return ESP_ERR_INVALID_STATE;
    }

    FILE *f = fopen(path, "w");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_FAIL;
    }

    // Write JSON (simple format for now)
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": \"%s\",\n", g_boot_config->version);
    fprintf(f, "  \"device_name\": \"%s\",\n", g_boot_config->device_name);
    fprintf(f, "  \"config_mode\": \"%s\",\n",
            g_boot_config->config_mode == CONFIG_MODE_AUTO_INIT ? "auto_init" : "minimal");
    fprintf(f, "  \"uart\": {\n");
    fprintf(f, "    \"console\": {\n");
    fprintf(f, "      \"num\": %d,\n", g_boot_config->uart.console.uart_num);
    fprintf(f, "      \"baudrate\": %d\n", g_boot_config->uart.console.baudrate);
    fprintf(f, "    }\n");
    fprintf(f, "  },\n");
    fprintf(f, "  \"features\": {\n");
    fprintf(f, "    \"shell\": %s,\n", g_boot_config->features.shell_enabled ? "true" : "false");
    fprintf(f, "    \"logging\": %s\n", g_boot_config->features.logging_enabled ? "true" : "false");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);

    ESP_LOGI(TAG, "✓ Config saved to %s", path);
    return ESP_OK;
}

void config_free(void)
{
    if (g_boot_config)
    {
        free(g_boot_config);
        g_boot_config = NULL;
    }
}

void config_print_summary(void)
{
    if (!g_boot_config)
    {
        ESP_LOGW(TAG, "No config loaded");
        return;
    }

    ESP_LOGI(TAG, "Configuration Summary:");
    ESP_LOGI(TAG, "  Version:     %s", g_boot_config->version);
    ESP_LOGI(TAG, "  Device:      %s", g_boot_config->device_name);
    ESP_LOGI(TAG, "  Mode:        %s",
             g_boot_config->config_mode == CONFIG_MODE_AUTO_INIT ? "AUTO_INIT" : "MINIMAL");
    ESP_LOGI(TAG, "  Console:     UART%d @ %d baud",
             g_boot_config->uart.console.uart_num,
             g_boot_config->uart.console.baudrate);
    ESP_LOGI(TAG, "  Shell:       %s", g_boot_config->features.shell_enabled ? "ON" : "OFF");
}
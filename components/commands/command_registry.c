/**
 * Command Registry Implementation
 */

#include "command_registry.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "CMD_REGISTRY";

#define MAX_COMMANDS 32

static command_t g_commands[MAX_COMMANDS];
static int g_command_count = 0;

void command_registry_init(void) {
    ESP_LOGI(TAG, "Command Registry initialized");
    g_command_count = 0;
}

esp_err_t command_register(const command_t* cmd) {
    if (g_command_count >= MAX_COMMANDS) {
        ESP_LOGE(TAG, "Registry full! Max %d commands", MAX_COMMANDS);
        return ESP_ERR_NO_MEM;
    }
    
    // Check for duplicates
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, cmd->name) == 0) {
            ESP_LOGW(TAG, "Command '%s' already registered", cmd->name);
            return ESP_ERR_INVALID_ARG;
        }
    }
    
    // Copy command
    memcpy(&g_commands[g_command_count], cmd, sizeof(command_t));
    g_command_count++;
    
    ESP_LOGI(TAG, "Registered: %s", cmd->name);
    
    return ESP_OK;
}

const command_t* command_find(const char* name) {
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, name) == 0) {
            return &g_commands[i];
        }
    }
    return NULL;
}

esp_err_t command_execute(const char* input) {
    // Parse command name
    char cmd_name[64];
    char args_str[512];
    
    // Split at first space or colon
    const char* sep = strpbrk(input, " :");
    if (sep) {
        size_t name_len = sep - input;
        if (name_len >= sizeof(cmd_name)) name_len = sizeof(cmd_name) - 1;
        strncpy(cmd_name, input, name_len);
        cmd_name[name_len] = '\0';
        
        // Rest is arguments
        strncpy(args_str, sep, sizeof(args_str) - 1);
        args_str[sizeof(args_str) - 1] = '\0';
    } else {
        // No arguments
        strncpy(cmd_name, input, sizeof(cmd_name) - 1);
        cmd_name[sizeof(cmd_name) - 1] = '\0';
        args_str[0] = '\0';
    }
    
    // Find command
    const command_t* cmd = command_find(cmd_name);
    if (!cmd) {
        printf("Command not found: %s\n", cmd_name);
        printf("Type 'help' for available commands.\n");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Parse arguments (simple tokenizer)
    char* argv[16];
    int argc = 0;
    
    argv[argc++] = cmd_name;  // argv[0] is command name
    
    if (strlen(args_str) > 0) {
        char* token = strtok(args_str, " :");
        while (token && argc < 16) {
            argv[argc++] = token;
            token = strtok(NULL, " :");
        }
    }
    
    // Execute
    return cmd->execute(argc, argv);
}

const command_t* command_get_all(int* count) {
    *count = g_command_count;
    return g_commands;
}
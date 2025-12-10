/**
 * Help Command
 */

#include "command_registry.h"
#include "esp_log.h"
#include <stdio.h>

static esp_err_t cmd_help_execute(int argc, char **argv)
{
    int count = 0;
    const command_t *commands = command_get_all(&count);

    if (argc > 1)
    {
        // Help for specific command
        const char *cmd_name = argv[1];
        const command_t *cmd = command_find(cmd_name);

        if (!cmd)
        {
            printf("Command not found: %s\n", cmd_name);
            return ESP_ERR_NOT_FOUND;
        }

        printf("\n");
        printf("Command: %s\n", cmd->name);
        printf("Description: %s\n", cmd->description);
        printf("Usage: %s\n", cmd->usage);
        printf("\n");
    }
    else
    {
        // List all commands
        printf("\n");
        printf("Available Commands (%d):\n", count);
        printf("------------------------\n");

        for (int i = 0; i < count; i++)
        {
            printf("  %-15s - %s\n", commands[i].name, commands[i].description);
        }

        printf("\n");
        printf("Type 'help <command>' for detailed help.\n");
        printf("\n");
    }

    return ESP_OK;
}

static const command_t cmd_help = {
    .name = "help",
    .description = "Show available commands",
    .usage = "help [command]",
    .execute = cmd_help_execute};

void register_builtin_commands(void)
{
    
    // EIGENTLICH SOLLTE DIESES PART NICHT HARCODIERT SEIN, ALLE COMMANDS SOLLTEN AUTOMATISCH DYNAMISCH
    // ERKANNT WERDEN , DIESER PART HIER WIRKT ÜBERFLÜSSIG
    command_register(&cmd_help);
    register_system_commands();
    register_gpio_commands();
    register_storage_commands();
    register_config_commands();
    register_adc_commands();
    register_pwm_commands();
    register_i2c_commands();
}
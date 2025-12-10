/**
 * Command Registry - Dynamic Command Management
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Command function signature
    typedef esp_err_t (*command_fn_t)(int argc, char **argv);

    // Command structure
    typedef struct
    {
        char name[32];
        char description[128];
        char usage[256];
        command_fn_t execute;
    } command_t;

    // EIGENTLICH SOLLTE DIESES PART NICHT HARCODIERT SEIN, ALLE COMMANDS SOLLTEN AUTOMATISCH DYNAMISCH
    // ERKANNT WERDEN , DIESER PART HIER WIRKT ÜBERFLÜSSIG
    /**
     * @brief Initialize command registry
     */
    void command_registry_init(void);

    /**
     * @brief Register a command
     */
    esp_err_t command_register(const command_t *cmd);

    /**
     * @brief Find command by name
     */
    const command_t *command_find(const char *name);

    /**
     * @brief Execute command from string
     */
    esp_err_t command_execute(const char *input);

    /**
     * @brief Get all registered commands
     */
    const command_t *command_get_all(int *count);

    /**
     * @brief Register builtin commands
     */
    void register_builtin_commands(void);

    /**
     * @brief Register system commands
     */
    void register_system_commands(void);

    /**
     * @brief Register GPIO commands
     */
    void register_gpio_commands(void);

    /**
     * @brief Register storage commands
     */
    void register_storage_commands(void);

    /**
     * @brief Register config commands
     */

    void register_config_commands(void);

    /**
     * @brief Register adc commands
     */

    void register_adc_commands(void);

    /**
     * @brief Register pwm commands
     */

    void register_pwm_commands(void);

    /**
     * @brief Register i2c commands
     */

    void register_i2c_commands(void);

#ifdef __cplusplus
}
#endif

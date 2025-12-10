idf_component_register(
    SRCS 
        "command_registry.c"
        "cmd_help.c"
        "cmd_system.c"
        "cmd_gpio.c"
        "cmd_storage.c"
        "cmd_config.c"
        "cmd_adc.c"
        "cmd_pwm.c"
        "cmd_i2c.c"
    INCLUDE_DIRS "."
    REQUIRES config driver esp_adc
)
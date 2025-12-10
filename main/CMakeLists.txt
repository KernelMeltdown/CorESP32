# Main Component
idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        storage
        config
        hardware_adapter
        shell
        commands
        utils
)
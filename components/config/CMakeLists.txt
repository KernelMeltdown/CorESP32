# File: components/config/CMakeLists.txt
idf_component_register(
    SRCS 
        "config_loader.c"
        "jsmn.c"
    INCLUDE_DIRS "."
    REQUIRES storage hardware_adapter
)
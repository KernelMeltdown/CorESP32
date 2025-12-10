# Boot Manager Component (renamed to avoid conflict with ESP-IDF bootloader)
idf_component_register(
    SRCS "dummy.c"
    INCLUDE_DIRS "."
)
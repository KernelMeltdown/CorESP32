# Storage Component - LittleFS Mount
idf_component_register(
    SRCS "littlefs_mount.c"
    INCLUDE_DIRS "."
    REQUIRES esp_partition joltwallet__littlefs
)
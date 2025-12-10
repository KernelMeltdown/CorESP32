# Hardware Adapter Component
idf_component_register(
    SRCS "hw_adapter.c"
    INCLUDE_DIRS "."
    REQUIRES driver config
)
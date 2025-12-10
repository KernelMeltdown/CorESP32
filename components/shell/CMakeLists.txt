# Shell Component
idf_component_register(
    SRCS "coreshell.c"
    INCLUDE_DIRS "."
    REQUIRES driver config commands
)
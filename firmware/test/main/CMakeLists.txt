idf_component_register(
    SRCS "../test_state_store.c"
    INCLUDE_DIRS "."
    REQUIRES unity state_store json
    WHOLE_ARCHIVE
)

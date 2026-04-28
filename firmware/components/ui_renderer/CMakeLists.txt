idf_component_register(
    SRCS
        "ui.c"
        "screens.c"
        "font_cjk.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl state_store
)

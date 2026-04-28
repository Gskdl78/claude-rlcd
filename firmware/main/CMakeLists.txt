idf_component_register(
    SRCS "app_main.c" "display_glue.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi net_manager state_store ui_renderer ws_server
             sensors_clock audio_alert display_bsp
)

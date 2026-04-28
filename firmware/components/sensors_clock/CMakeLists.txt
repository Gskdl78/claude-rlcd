idf_component_register(
    SRCS "sclk.c"
    INCLUDE_DIRS "include"
    REQUIRES driver lwip esp_netif log
)

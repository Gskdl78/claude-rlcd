idf_component_register(
    SRCS "net.c"
    INCLUDE_DIRS "include"
    EMBED_FILES "captive.html"
    REQUIRES esp_wifi esp_http_server esp_netif nvs_flash mdns esp_event esp_event log
)

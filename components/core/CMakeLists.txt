idf_component_register(
    SRCS
        "app.cpp"
        "wifi_manager.cpp"
        "config.cpp"
        "shared_state.cpp"
        "module_manager.cpp"
        "ui_schema.cpp"
        "event_bus.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        littlefs
        nvs_flash
        esp_event
        esp_netif
        esp_wifi
        esp_http_server
        log
        json
        esp_system
        lwip
        freertos
)
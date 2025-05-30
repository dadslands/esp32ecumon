idf_component_register(SRCS "app_main.c"
                      "wifi_manager.c"
                      "http_server_handler.c"
                INCLUDE_DIRS "."
                REQUIRES esp_wifi # For Wi-Fi functions
                         esp_event # For event loop
                         esp_netif # For network interface
                         nvs_flash # For NVS storage
                         esp_http_server # For HTTP server
                         esp_log # For logging
                         freertos # For FreeRTOS features
                         cJSON # For JSON parsing/creation in HTTP handlers
                )

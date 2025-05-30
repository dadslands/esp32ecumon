idf_component_register(SRCS "hid_handler.c"
                    INCLUDE_DIRS "."
                    REQUIRES rmt # For WS2812B (Neopixel) LEDs via RMT
                             led_strip # ESP-IDF component for LED strips
                             can_reader # To access can_reader_get_latest_rpm()
                             freertos # For FreeRTOS features like mutex and tasks
                    # Add other dependencies if HID keyboard functionality is fully added later
                    # e.g., esp_hid, esp_usb_host
                    )

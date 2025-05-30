# CMakeLists.txt for bb_spi_lcd_driver component

idf_component_register(SRCS "bb_spi_lcd.cpp"  # Changed from .c to .cpp
                    INCLUDE_DIRS "."
                    REQUIRES driver          # For ESP-IDF SPI master, GPIO
                             esp_log         # For ESP_LOG logging
                             freertos        # For FreeRTOS types if used by driver (e.g. delay)
                    # The library internally includes Arduino.h and SPI.h for Arduino targets.
                    # For ESP-IDF, it uses driver/spi_master.h and driver/gpio.h directly.
                    # Ensure ESP-IDF toolchain handles C++ compilation for .cpp files.
                    )

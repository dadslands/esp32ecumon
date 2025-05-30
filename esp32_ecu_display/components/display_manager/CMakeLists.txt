idf_component_register(SRCS "display_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver # For SPI master driver (driver/spi_master.h), gpio.h
                             esp_log
                             freertos # For tasks, semaphores
                             bb_spi_lcd_driver # The new component for the display driver
                             can_reader # To get data like RPM
                    # When LVGL is fully integrated, it would be added here too.
                    # For example:
                    # REQUIRES lvgl esp_lcd_panel_io
                    )

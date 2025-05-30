idf_component_register(SRCS "can_reader.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver # For TWAI (driver/twai.h) and GPIO (driver/gpio.h)
                             freertos # For FreeRTOS tasks and types
                    # esp_log is usually implicitly available
                    )

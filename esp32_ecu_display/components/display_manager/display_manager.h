#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "esp_err.h"

/**
 * @brief Initializes the Display Manager module.
 *
 * This function sets up the SPI bus for communication with the display and
 * initializes the GC9A01 display controller using the `bb_spi_lcd` library.
 * It configures the necessary GPIO pins for SPI (SCLK, MOSI) and display control (CS, DC, RST).
 * After successful initialization, the screen is typically cleared.
 *
 * @note This function relies on the `bb_spi_lcd` component being available and configured.
 *       The actual `bb_spi_lcd.c` (or `.cpp`) from the library must be used, not the stub.
 *
 * @return esp_err_t ESP_OK on successful initialization, or an error code on failure.
 */
esp_err_t display_manager_init(void);

/**
 * @brief FreeRTOS task for managing display updates.
 *
 * This task periodically retrieves data (e.g., IP address from `wifi_manager`,
 * RPM from `can_reader`) and updates the GC9A01 display using functions
 * from the `bb_spi_lcd` library (e.g., `spilcdFill`, `spilcdWriteString`).
 * It handles basic formatting and layout for the displayed information.
 *
 * @param pvParameters Task parameters (not used).
 */
void display_manager_task(void *pvParameters);

#endif // DISPLAY_MANAGER_H

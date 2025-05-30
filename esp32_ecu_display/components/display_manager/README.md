# Display Manager Component

## Overview

The `display_manager` component is responsible for managing the TFT LCD display attached to the ESP32. Its primary role is to initialize the display hardware and provide functionalities to draw data and user interface elements onto the screen. For this project, it uses the `bb_spi_lcd` driver component to interface with a GC9A01 round display (240x240).

## Key Responsibilities

*   **Display Initialization:**
    *   Configures and initializes the SPI bus (SPI2_HOST) used for communication with the display.
    *   Initializes the GC9A01 display controller using the `bb_spi_lcd` library, including setting up control pins (CS, DC, RST) and display parameters (resolution, controller type).
    *   Clears the screen or fills it with a default background color upon initialization.
*   **Data Rendering:**
    *   Runs a FreeRTOS task (`display_manager_task`) that periodically fetches data from other components (e.g., IP address from `wifi_manager`, RPM from `can_reader`).
    *   Uses drawing functions provided by the `bb_spi_lcd` library (e.g., `spilcdFill`, `spilcdWriteString`) to render text and potentially graphical elements on the display.
    *   Handles basic formatting of data for display.
*   **User Interface Management (Future):**
    *   While currently displaying simple text, this component would be the place to integrate a more advanced graphics library like LVGL for creating sophisticated user interfaces with widgets, charts, and animations.

## Public API

Key public functions are declared in `display_manager.h`:

*   `esp_err_t display_manager_init(void);`
    *   Initializes the SPI bus and the GC9A01 display.
*   `void display_manager_task(void *pvParameters);`
    *   The FreeRTOS task function that handles periodic screen updates.

## Configuration (GC9A01 Specific)

*   **Display Controller:** GC9A01
*   **Resolution:** 240x240 pixels
*   **Interface:** SPI (via `bb_spi_lcd` driver)
*   **SPI Host:** `SPI2_HOST`
*   **Pins:**
    *   SCLK: GPIO 14
    *   MOSI (SDA): GPIO 15
    *   CS (Chip Select): GPIO 5
    *   DC (Data/Command): GPIO 4
    *   RST (Reset): GPIO 12
    *   Backlight (BCKL): Not directly controlled by this component in the current configuration (assumed always on or externally managed).

This component abstracts the details of display communication and rendering, providing a clear interface for showing data to the user.

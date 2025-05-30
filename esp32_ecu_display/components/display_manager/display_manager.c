#include "display_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// SPI master driver is used by bb_spi_lcd internally for ESP-IDF
// #include "driver/spi_master.h"
#include "driver/gpio.h"      // For manual RST pin control
#include "bb_spi_lcd.h"       // Interface to the bb_spi_lcd library
#include "can_reader.h"       // To get RPM data
#include "wifi_manager.h"     // To get IP address string
#include <stdio.h>            // For snprintf

static const char *TAG = "display_manager";

// GC9A01 Display & SPI Configuration
// These pins are passed to spilcdInit.
#define LCD_SPI_HOST      SPI2_HOST // ESP32 has SPI0, SPI1 (FSPI), SPI2 (HSPI), SPI3 (VSPI)
                                  // HSPI/VSPI are common choices. SPI2_HOST is HSPI.
#define LCD_PIN_NUM_SCLK  GPIO_NUM_14
#define LCD_PIN_NUM_MOSI  GPIO_NUM_15
#define LCD_PIN_NUM_MISO  -1        // MISO not used for display-only communication with GC9A01
#define LCD_PIN_NUM_CS    GPIO_NUM_5
#define LCD_PIN_NUM_DC    GPIO_NUM_4
#define LCD_PIN_NUM_RST   GPIO_NUM_12
#define LCD_PIN_NUM_BCKL  -1        // Backlight control: -1 if not controlled or always on.
                                  // If controlled, assign a GPIO and manage via spilcdSetBrightness or direct GPIO.

#define LCD_SPI_CLOCK_SPEED_HZ (40 * 1000 * 1000) // 40 MHz. GC9A01 supports up to ~60-80MHz depending on board.

// Global instance of the display device structure from bb_spi_lcd library
static SPILCD s_lcd_dev;

/**
 * @brief Initializes the Display Manager module.
 * (Public function documentation is in display_manager.h)
 */
esp_err_t display_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Display Manager module with bb_spi_lcd driver...");
    esp_err_t ret_esp = ESP_OK;

    // Perform manual hardware reset for the display if RST pin is defined
    if (LCD_PIN_NUM_RST >= 0) {
        ESP_LOGI(TAG, "Performing manual reset for display on GPIO %d", LCD_PIN_NUM_RST);
        gpio_config_t rst_gpio_config = {
            .pin_bit_mask = (1ULL << LCD_PIN_NUM_RST),
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&rst_gpio_config));

        gpio_set_level((gpio_num_t)LCD_PIN_NUM_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(100)); // Hold reset low
        gpio_set_level((gpio_num_t)LCD_PIN_NUM_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(120)); // Wait for display to stabilize
    }

    // Initialize the LCD using the C API from bb_spi_lcd
    // The spilcdInit function will handle SPI bus initialization and device registration for ESP-IDF.
    ESP_LOGI(TAG, "Calling spilcdInit for GC9A01 display controller...");
    int init_ret = spilcdInit(&s_lcd_dev,
                              LCD_GC9A01,             // Display controller type
                              FLAGS_NONE,             // Initialization flags (e.g., color swap, inversion)
                              LCD_SPI_CLOCK_SPEED_HZ, // SPI clock speed
                              LCD_PIN_NUM_CS,         // Chip Select pin
                              LCD_PIN_NUM_DC,         // Data/Command pin
                              LCD_PIN_NUM_RST,        // Reset pin (-1 if not used by driver init)
                              LCD_PIN_NUM_BCKL,       // Backlight pin (-1 if not used)
                              LCD_PIN_NUM_MISO,       // MISO pin (-1 if not used)
                              LCD_PIN_NUM_MOSI,       // MOSI pin
                              LCD_PIN_NUM_SCLK,       // SCLK pin
                              1);                     // bUseDMA: 1 for ESP32 to use its SPI driver features

    if (init_ret != 0) { // bb_spi_lcd C API typically returns 0 for success
        ESP_LOGE(TAG, "spilcdInit failed with error code: %d", init_ret);
        // SPI bus might have been partially initialized by spilcdInit; proper cleanup might be needed
        // if spilcdInit doesn't handle its own full cleanup on error.
        return ESP_FAIL;
    }

    // Ensure display dimensions are correctly set in the SPILCD struct after init
    // (spilcdInit should set these based on iLCDType)
    ESP_LOGI(TAG, "Display initialized. Type: %d, Native W: %d, H: %d. Current W: %d, H: %d",
             s_lcd_dev.iLCDType, s_lcd_dev.iWidth, s_lcd_dev.iHeight,
             s_lcd_dev.iCurrentWidth, s_lcd_dev.iCurrentHeight);

    // Set default orientation (optional, spilcdInit might set a default)
    // spilcdSetOrientation(&s_lcd_dev, LCD_ORIENTATION_0);

    // Fill screen with black to clear it
    ESP_LOGI(TAG, "Clearing screen (filling with black)...");
    spilcdFill(&s_lcd_dev, TFT_BLACK, DRAW_TO_LCD); // Use color constant from bb_spi_lcd.h

    ESP_LOGI(TAG, "Display Manager initialized successfully.");
    return ret_esp; // Should be ESP_OK if all above succeeded
}

/**
 * @brief FreeRTOS task for managing display updates.
 * (Public function documentation is in display_manager.h)
 */
void display_manager_task(void *pvParameters) {
    ESP_LOGI(TAG, "Display Manager Task started.");
    char ip_address_buffer[48]; // Buffer for "IP: xxx.xxx.xxx.xxx" or status messages
    char rpm_buffer[20];      // Buffer for "RPM: XXXX"
    char temp_display_buffer[64];  // General purpose buffer for formatting text before drawing

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t task_frequency = pdMS_TO_TICKS(1000); // Update display every 1 second

    // Brief delay to allow other system components (like Wi-Fi) to potentially initialize
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        // 1. Gather data to display
        wifi_manager_get_ip_info_str(temp_display_buffer, sizeof(temp_display_buffer));
        snprintf(ip_address_buffer, sizeof(ip_address_buffer), "IP: %s", temp_display_buffer);

        float current_rpm = can_reader_get_latest_rpm();
        snprintf(rpm_buffer, sizeof(rpm_buffer), "RPM: %.0f", current_rpm);

        // 2. Prepare display: Clear screen (or relevant parts)
        // For simplicity, clearing the whole screen. In a more complex UI,
        // only areas that change would be cleared and redrawn.
        spilcdFill(&s_lcd_dev, TFT_BLACK, DRAW_TO_LCD);

        // 3. Draw IP Address
        // The spilcdWriteString function uses the iFG, iBG, and iFont members of the SPILCD struct.
        // These can be set once or before each call if different styles are needed.
        // For ESP-IDF C API usage, it's often easier to pass colors/font directly if function allows,
        // or ensure they are set in SPILCD struct before calling.
        // The C API spilcdWriteString takes fg, bg, font as params.
        spilcdWriteString(&s_lcd_dev,
                          5, 10,                       // x, y coordinates
                          ip_address_buffer,            // Text to display
                          TFT_GREEN, TFT_BLACK,       // Foreground, Background colors
                          FONT_6x8,                   // Font size (from bb_spi_lcd.h enum)
                          DRAW_TO_LCD);               // Render flags

        // 4. Display RPM
        spilcdWriteString(&s_lcd_dev,
                          5, 30,                       // x, y coordinates
                          rpm_buffer,                   // Text to display
                          TFT_WHITE, TFT_BLACK,       // Foreground, Background colors
                          FONT_8x8,                   // Font size
                          DRAW_TO_LCD);               // Render flags

        ESP_LOGD(TAG, "Display updated: %s, %s", ip_address_buffer, rpm_buffer);

        // The bb_spi_lcd library functions like spilcdWriteString typically handle the actual drawing
        // to the display immediately or to a backbuffer if configured (not used in this basic setup).
        // No explicit "refresh" call is usually needed unless using its backbuffer features and spilcdShowBuffer.

        vTaskDelayUntil(&last_wake_time, task_frequency); // Wait for the next update cycle
    }
}

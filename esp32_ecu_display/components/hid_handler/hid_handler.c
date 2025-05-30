#include "hid_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/rmt.h"    // For RMT peripheral control (used by led_strip)
#include "led_strip.h"   // For WS2812B (NeoPixel) control
#include "can_reader.h"  // To get RPM for high RPM warning

static const char *TAG = "hid_controller";

// LED Configuration
#define LED_STRIP_GPIO GPIO_NUM_48       // GPIO pin for the WS2812B LED
#define LED_STRIP_RMT_CHANNEL RMT_CHANNEL_0 // RMT channel for WS2812B communication
                                         // Note: For ESP-IDF 5.x, RMT channel is managed more internally by led_strip.
static led_strip_handle_t s_led_strip = NULL; // Handle for the LED strip driver

// System state for LED control - protected by a mutex
static system_led_state_t s_current_led_system_state = SYS_LED_STATE_BOOTING;
static SemaphoreHandle_t s_led_state_mutex = NULL;

// RPM threshold for high RPM warning
#define HIGH_RPM_THRESHOLD 4500.0f

// --- Static Helper Functions ---

/**
 * @brief Sets the color of the RGB LED.
 * Does not refresh the strip; led_strip_refresh must be called separately.
 * @param r Red component (0-255).
 * @param g Green component (0-255).
 * @param b Blue component (0-255).
 * @return esp_err_t ESP_OK on success, ESP_FAIL if strip not initialized.
 */
static esp_err_t hid_led_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (s_led_strip) {
        return led_strip_set_pixel(s_led_strip, 0, r, g, b);
    }
    return ESP_FAIL;
}

/**
 * @brief Clears the RGB LED (turns it off).
 * Does not refresh the strip; led_strip_refresh must be called separately.
 * @return esp_err_t ESP_OK on success, ESP_FAIL if strip not initialized.
 */
static esp_err_t hid_led_clear(void) {
    if (s_led_strip) {
        return led_strip_clear(s_led_strip);
    }
    return ESP_FAIL;
}


// --- Public API Functions ---

/**
 * @brief Initializes the LED controller.
 * (Public function documentation is in hid_handler.h)
 */
esp_err_t hid_led_controller_init(void) {
    ESP_LOGI(TAG, "Initializing LED controller (WS2812B on GPIO %d)...", LED_STRIP_GPIO);

    s_led_state_mutex = xSemaphoreCreateMutex();
    if (s_led_state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LED state mutex!");
        return ESP_FAIL;
    }

    // Configuration for the WS2812B LED strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,       // The GPIO pin connected to the data line of the WS2812B
        .max_leds = 1,                          // Number of LEDs in the strip
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of WS2812B (GRB or RGB)
        .led_model = LED_MODEL_WS2812,          // LED model
        .flags.invert_out = false,              // True if the output signal needs to be inverted
    };

    // RMT (Remote Control Transceiver) specific configuration for the LED strip
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,         // Clock source for RMT. Default is APB CLK.
        .resolution_hz = 10 * 1000 * 1000,      // RMT counter clock resolution (10MHz for WS2812B)
        .mem_block_symbols = 0,                 // ESP-IDF 5.x: Auto-selects RMT memory blocks if 0.
                                                // ESP-IDF 4.x: e.g., 64 for one channel.
        .flags.with_dma = false,                // DMA not critical for a single LED, can be true for longer strips
    };
    // rmt_config.channel = LED_STRIP_RMT_CHANNEL; // For ESP-IDF v4.x RMT driver. Not used in ESP-IDF v5.x led_strip component this way.

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create new RMT LED strip: %s", esp_err_to_name(err));
        vSemaphoreDelete(s_led_state_mutex);
        s_led_state_mutex = NULL;
        s_led_strip = NULL;
        return err;
    }

    // Clear LED on successful initialization
    err = hid_led_clear();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to clear LED strip during init: %s", esp_err_to_name(err));
    }
    err = led_strip_refresh(s_led_strip); // Apply the clear operation
     if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to refresh LED strip during init clear: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "LED controller initialized successfully.");
    return ESP_OK;
}

/**
 * @brief Sets the desired system state for LED indication.
 * (Public function documentation is in hid_handler.h)
 */
esp_err_t hid_led_controller_set_system_state(system_led_state_t new_state) {
    if (s_led_state_mutex == NULL) {
        ESP_LOGE(TAG, "LED state mutex not initialized in set_system_state.");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_led_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) { // Wait up to 50ms for the mutex
        if (s_current_led_system_state != new_state) {
            ESP_LOGI(TAG, "Setting LED system state from %d to %d", s_current_led_system_state, new_state);
            s_current_led_system_state = new_state;
        }
        xSemaphoreGive(s_led_state_mutex);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to take LED state mutex in set_system_state within 50ms.");
        return ESP_FAIL; // Indicate failure to set state
    }
}

/**
 * @brief FreeRTOS task to manage LED patterns.
 * (Public function documentation is in hid_handler.h)
 */
void hid_led_controller_task(void *pvParameters) {
    ESP_LOGI(TAG, "LED Controller Task started.");
    system_led_state_t current_task_state = SYS_LED_STATE_BOOTING; // Local copy of the state the task is currently displaying
    system_led_state_t prev_displayed_task_state = SYS_LED_STATE_OFF; // Previously displayed state to optimize refreshes
    bool led_on_for_blink = false; // Toggles for blinking patterns
    uint32_t sta_connected_start_time_ticks = 0; // Timestamp for STA_CONNECTED_TEMP duration
    float current_rpm_value = 0;
    bool high_rpm_active = false;

    TickType_t last_wake_time = xTaskGetTickCount();
    // Base update frequency. Blinking/pulsing effects are tied to this.
    const TickType_t task_frequency_ticks = pdMS_TO_TICKS(150);

    if (s_led_strip == NULL) {
        ESP_LOGE(TAG, "LED strip handle is NULL at start of LED task. Deleting mutex and exiting task.");
        if(s_led_state_mutex) vSemaphoreDelete(s_led_state_mutex);
        s_led_state_mutex = NULL;
        vTaskDelete(NULL); // Delete self
        return;
    }

    while (1) {
        // Atomically get the current global system state
        if (s_led_state_mutex && xSemaphoreTake(s_led_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            current_task_state = s_current_led_system_state;
            xSemaphoreGive(s_led_state_mutex);
        } else {
            ESP_LOGE(TAG, "LED task failed to get state mutex. Using last known state: %d", current_task_state);
        }

        // Check for RPM override condition
        current_rpm_value = can_reader_get_latest_rpm();
        high_rpm_active = (current_rpm_value > HIGH_RPM_THRESHOLD);

        system_led_state_t state_to_display = current_task_state; // Start with the global state

        // Apply RPM override if conditions are met
        if (high_rpm_active &&
            (current_task_state == SYS_LED_STATE_STA_IDLE ||
             current_task_state == SYS_LED_STATE_STA_CONNECTED_TEMP ||
             current_task_state == SYS_LED_STATE_STA_CONNECTING)) {
            state_to_display = SYS_LED_STATE_HIGH_RPM_WARNING;
        } else if (!high_rpm_active && prev_displayed_task_state == SYS_LED_STATE_HIGH_RPM_WARNING) {
            // If RPM warning was active, but RPM is now low, revert to the actual current system state.
            state_to_display = current_task_state;
        }

        // Only update LED if the effective state has changed OR it's a dynamic (blinking/pulsing) state
        // This minimizes unnecessary led_strip_set_pixel and led_strip_refresh calls for solid states.
        bool needs_update = (state_to_display != prev_displayed_task_state) ||
                            (state_to_display == SYS_LED_STATE_AP_MODE) ||
                            (state_to_display == SYS_LED_STATE_STA_CONNECTING) ||
                            (state_to_display == SYS_LED_STATE_HIGH_RPM_WARNING) ||
                            (state_to_display == SYS_LED_STATE_STA_CONNECTED_TEMP); // STA_CONNECTED_TEMP needs continuous check for timeout

        if (needs_update) {
            switch (state_to_display) {
                case SYS_LED_STATE_BOOTING:
                    hid_led_set_color(50, 50, 50); // Dim White
                    break;
                case SYS_LED_STATE_AP_MODE: // Pulsing Blue
                    if (led_on_for_blink) hid_led_set_color(0, 0, 120); else hid_led_clear();
                    led_on_for_blink = !led_on_for_blink;
                    break;
                case SYS_LED_STATE_STA_CONNECTING: // Blinking Yellow
                    if (led_on_for_blink) hid_led_set_color(120, 80, 0); else hid_led_clear();
                    led_on_for_blink = !led_on_for_blink;
                    break;
                case SYS_LED_STATE_STA_CONNECTED_TEMP:
                    hid_led_set_color(0, 120, 0); // Solid Green
                    if (sta_connected_start_time_ticks == 0) {
                        sta_connected_start_time_ticks = xTaskGetTickCount();
                    }
                    // Check if 3 seconds have passed
                    if ((xTaskGetTickCount() - sta_connected_start_time_ticks) >= pdMS_TO_TICKS(3000)) {
                         // Check global state before transitioning to prevent overriding a newer state
                         if (s_current_led_system_state == SYS_LED_STATE_STA_CONNECTED_TEMP) {
                            hid_led_controller_set_system_state(SYS_LED_STATE_STA_IDLE);
                         }
                         sta_connected_start_time_ticks = 0; // Reset timer for next time
                    }
                    break;
                case SYS_LED_STATE_STA_IDLE:
                    hid_led_clear(); // LED Off
                    sta_connected_start_time_ticks = 0; // Reset timer if we enter idle
                    break;
                case SYS_LED_STATE_STA_FAILED:
                    hid_led_set_color(120, 0, 0); // Solid Red
                    break;
                case SYS_LED_STATE_HIGH_RPM_WARNING:
                    if (led_on_for_blink) hid_led_set_color(180, 0, 0); else hid_led_clear(); // Flashing Red
                    led_on_for_blink = !led_on_for_blink;
                    break;
                case SYS_LED_STATE_OFF:
                default:
                    hid_led_clear();
                    break;
            }

            prev_displayed_task_state = state_to_display; // Update the last displayed state

            if (s_led_strip) { // Ensure strip handle is still valid
                esp_err_t refresh_err = led_strip_refresh(s_led_strip);
                if (refresh_err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(refresh_err));
                }
            }
        }
        vTaskDelayUntil(&last_wake_time, task_frequency_ticks);
    }
}

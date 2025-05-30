#ifndef HID_HANDLER_H
#define HID_HANDLER_H

#include "esp_err.h"

/**
 * @brief Enum defining system states for LED indication.
 *
 * These states are used by various parts of the application to signal
 * the desired visual feedback to be shown by the RGB LED.
 */
typedef enum {
    SYS_LED_STATE_BOOTING,          /*!< ESP32 is booting up. */
    SYS_LED_STATE_AP_MODE,          /*!< Wi-Fi Access Point mode is active. */
    SYS_LED_STATE_STA_CONNECTING,   /*!< Wi-Fi Station mode is attempting to connect to an AP. */
    SYS_LED_STATE_STA_CONNECTED_TEMP,/*!< Wi-Fi Station successfully connected (temporary state, e.g., for solid green). */
    SYS_LED_STATE_STA_IDLE,         /*!< Wi-Fi Station connected and idle (e.g., LED off or subtle). */
    SYS_LED_STATE_STA_FAILED,       /*!< Wi-Fi Station failed to connect. */
    SYS_LED_STATE_HIGH_RPM_WARNING, /*!< Engine RPM has exceeded the defined threshold. */
    SYS_LED_STATE_OFF               /*!< Explicitly turn LED off. */
} system_led_state_t;

/**
 * @brief Initializes the LED controller part of the HID handler.
 *
 * Configures the RMT peripheral and the `led_strip` driver for controlling
 * a WS2812B (NeoPixel) RGB LED connected to the defined GPIO pin.
 * It also creates a mutex for thread-safe state updates.
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t hid_led_controller_init(void);

/**
 * @brief Sets the desired system state for LED indication.
 *
 * This function is thread-safe and can be called from other tasks/modules
 * to signal changes in the overall system status, which the LED controller task
 * will then use to display appropriate visual feedback.
 *
 * @param new_state The new system state to be indicated by the LED.
 * @return esp_err_t ESP_OK if the state was updated successfully, ESP_FAIL if mutex could not be taken,
 *         or ESP_ERR_INVALID_STATE if the LED controller is not initialized.
 */
esp_err_t hid_led_controller_set_system_state(system_led_state_t new_state);

/**
 * @brief FreeRTOS task to manage LED patterns.
 *
 * This task periodically checks the current system state (set via
 * `hid_led_controller_set_system_state`) and the engine RPM (via `can_reader_get_latest_rpm`).
 * It then updates the RGB LED to display patterns (solid, blinking, pulsing)
 * corresponding to these states, including a high RPM warning override.
 *
 * @param pvParameters Task parameters (not used).
 */
void hid_led_controller_task(void *pvParameters);

#endif // HID_HANDLER_H

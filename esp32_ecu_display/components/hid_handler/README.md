# HID Handler (LED Controller) Component

## Overview

The `hid_handler` component is designed to manage Human Interface Devices (HID) for the ESP32 ECU Data Display project. Currently, its primary responsibility is to control an RGB LED (WS2812B NeoPixel) to provide visual status feedback about the system's operation. Future enhancements could include handling inputs from physical buttons, rotary encoders, or USB HID devices like keyboards.

## Key Responsibilities (Current - LED Control)

*   **LED Initialization:**
    *   Configures and initializes the ESP32's RMT (Remote Control) peripheral to drive the WS2812B LED.
    *   Uses the `led_strip` ESP-IDF component for abstracting NeoPixel control.
    *   Sets up the specified GPIO pin for LED data output.
*   **State Management:**
    *   Maintains the current system state relevant for LED indication (e.g., booting, AP mode, STA connecting, RPM warning) through a thread-safe mechanism (mutex).
    *   Provides a function (`hid_led_controller_set_system_state`) for other components to update this state.
*   **LED Pattern Generation:**
    *   Runs a FreeRTOS task (`hid_led_controller_task`) that periodically checks the current system state and other conditions (like engine RPM).
    *   Implements logic to display different LED patterns (solid colors, blinking, pulsing) based on the determined state.
    *   Handles overrides, such as a high RPM warning that takes precedence over some other states.

## Public API

Key public functions for LED control are declared in `hid_handler.h`:

*   `esp_err_t hid_led_controller_init(void);`
    *   Initializes the LED controller, RMT, and `led_strip` driver.
*   `esp_err_t hid_led_controller_set_system_state(system_led_state_t new_state);`
    *   Allows other parts of the application to set the desired LED indication state.
*   `void hid_led_controller_task(void *pvParameters);`
    *   The FreeRTOS task that manages the LED display patterns.

## Configuration (LED Specific)

*   **LED Type:** WS2812B (NeoPixel)
*   **Number of LEDs:** 1
*   **Data Pin:** GPIO 48
*   **RMT Channel:** `RMT_CHANNEL_0` (or as configured by `led_strip` component)

This component provides immediate visual feedback on the system's status, aiding in debugging and user interaction.

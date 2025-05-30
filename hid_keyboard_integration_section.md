## Part 6: HID Keyboard Integration for User Control

While our display will show valuable real-time data, adding user interaction can significantly enhance its functionality. Imagine cycling through different data screens, resetting trip meters, or pausing live data with the press of a key. This section explores how to integrate a standard USB Human Interface Device (HID) keyboard to control your ESP32 ECU Data Display. This functionality will be primarily managed within your `hid_handler.c` module.

### 6.1 Mapping Inputs: From Key Presses to Actions

To use a USB keyboard with your ESP32, the ESP32 needs to act as a **USB Host**. This is a bit more advanced than when the ESP32 acts as a USB Device (like when you program it). Some ESP32 variants (like ESP32-S2, ESP32-S3) have built-in USB On-The-Go (OTG) capabilities that can support USB Host mode more directly. For standard ESP32-WROOM-32 modules, you might need an external USB Host controller module (e.g., based on the MAX3421E chip) or a specific ESP32 board designed with USB Host/OTG hardware.

**6.1.1 Introduction to USB Host on ESP32**

*   **ESP32 as USB Host:** In Host mode, the ESP32 controls the USB bus and connected USB devices, like a keyboard. This is different from the default Device mode where your PC is the host.
*   **Hardware Requirements:**
    *   **ESP32-S2/S3:** These chips have native USB OTG support. You'll need to ensure your board breaks out the USB D+ and D- lines appropriately and provides VBUS power (5V) to the connected USB device.
    *   **Standard ESP32 (WROOM/WROVER):** Generally lacks native USB Host. You'd typically use an external USB Host controller IC (like MAX3421E) connected to the ESP32 via SPI. This IC handles the low-level USB protocol.
    *   **For this guide's context, we'll assume you're using an ESP32 variant or an external module that enables USB Host functionality.** The "Dual USB Support" feature mentioned earlier could involve one USB port for programming (Device mode) and the other configured for Host mode input.
*   **USB Host Library/Driver:**
    *   You'll need a software library to manage USB Host operations and specifically to handle HID class devices (keyboards, mice).
    *   **ESP-IDF:** The ESP-IDF often includes USB Host drivers, or components for USB Host support might be available through the ESP-IDF Component Registry or from community projects on platforms like GitHub (e.g., libraries built around `esp_usb_host`). Search for "ESP32 USB Host HID example" or check the official Espressif documentation and component registry for suitable libraries.
    *   These libraries typically handle the low-level enumeration of the USB device (identifying it as a keyboard) and provide a way to receive input reports (data packets indicating key presses).

**6.1.2 Detecting Key Presses**

Once the USB Host library is initialized and a keyboard is connected, the library will provide mechanisms to inform your application about key events. This might be through:

*   **Callback Functions:** You register a function that the library calls whenever a key is pressed or released. This callback usually provides information about the key, such as its usage code (e.g., `HID_KEY_A`, `HID_KEY_F1`, `HID_KEY_ARROW_UP`).
*   **Polling:** You might need to periodically call a function in the library to check if any new key events are available.

The data you receive will typically include:
*   **Key Code:** A numerical representation of the key pressed (e.g., standard HID usage codes).
*   **Modifiers:** Information about whether Shift, Ctrl, Alt, or GUI (Windows/Command) keys were held down during the key press.
*   **State:** Whether the key was pressed or released.

Your `hid_handler.c` will be responsible for:
1.  Initializing the USB Host stack and the HID driver.
2.  Processing the key event data provided by the library.
3.  Translating these raw key events into meaningful actions for your application.

**6.1.3 Assigning Shortcuts: From Keys to Actions**

This is where you define what each key press does. You'll create a mapping between specific key codes and the functions they trigger in your ECU data display.

**Example: Using F1 to Cycle Through Display Screens**

Let's say you want the F1 key to cycle through different screens or pages of data on your LVGL display. Your `display_manager.c` might have a variable like `current_screen` and a defined `TOTAL_SCREENS`.

Inside your `hid_handler.c` (or a callback function it manages), you would have logic similar to this:

```c
// --- In hid_handler.h (or a shared header) ---
// Define symbolic names for key codes provided by your USB HID Host library
// These values depend on the specific library you use.
// Example placeholder values - replace with actual values from your HID library!
#define HID_KEY_F1       0x3A // Example HID usage code for F1
#define HID_KEY_ARROW_UP 0x52 // Example for Arrow Up
// ... other key definitions

// --- In display_manager.h (or a shared header if hid_handler needs it) ---
extern int current_screen;     // Declare current_screen if defined in display_manager.c
extern const int TOTAL_SCREENS; // Declare TOTAL_SCREENS if defined in display_manager.c

// --- In hid_handler.c ---
// This function would be called by the USB Host library upon a key event,
// or you'd call a library function that returns the key_pressed.
// The exact signature and how key_pressed is obtained depends on the library.

// Assume these are defined and managed elsewhere (e.g., display_manager.c)
// int current_screen = 0;
// const int TOTAL_SCREENS = 3; // Example: 0, 1, 2

void process_key_press(uint8_t key_pressed) { // key_pressed is the code from USB HID lib
    switch (key_pressed) {
        case HID_KEY_F1:
            current_screen = (current_screen + 1) % TOTAL_SCREENS;
            // You might also need to call a function to trigger the actual screen update in LVGL
            // e.g., lvgl_screen_update_request(); or directly tell display_manager to change screen.
            printf("F1 pressed: Switching to screen %d\n", current_screen);
            break;

        case HID_KEY_ARROW_UP:
            // Example: Increase a value or scroll
            printf("Arrow Up pressed: Action for UP\n");
            // Call a function to handle "scroll up" or "increment value" on the current screen
            break;

        // Add more cases for other keys:
        // case HID_KEY_ARROW_DOWN: ...
        // case HID_KEY_ENTER: ...
        // etc.

        default:
            // Optional: Handle other keys or ignore them
            break;
    }
}

// --- Example of how this might be used with a hypothetical library task ---
// void usb_hid_task(void *pvParameters) {
//     initialize_usb_host_hid_library();
//     for (;;) {
//         if (is_keyboard_connected()) {
//             uint8_t received_key_code = get_next_key_press_from_library(); // Hypothetical
//             if (received_key_code != 0) { // 0 might mean no key pressed
//                 process_key_press(received_key_code);
//             }
//         }
//         vTaskDelay(pdMS_TO_TICKS(20)); // Poll every 20ms
//     }
// }
```

**Explanation of the F1 key example:**

*   **`key_pressed == HID_KEY_F1`**:
    *   `key_pressed`: This variable would hold the code for the key that was just pressed, as reported by the USB Host HID library.
    *   `HID_KEY_F1`: This is a symbolic constant (a `#define`) that you would typically define in a header file. Its actual value (e.g., `0x3A` in the comment) corresponds to the standard HID usage code for the F1 key. Your chosen USB Host library should provide documentation or header files listing these codes.
*   **`current_screen = (current_screen + 1) % TOTAL_SCREENS;`**:
    *   `current_screen`: This global variable (or a variable accessible to both `hid_handler.c` and `display_manager.c`) keeps track of which data page is currently active on the display.
    *   `TOTAL_SCREENS`: A constant representing the total number of available display pages (e.g., if you have 3 screens, this would be 3).
    *   The expression `(current_screen + 1) % TOTAL_SCREENS` is a common way to cycle through a sequence of numbers. It increments `current_screen`. If `current_screen` reaches `TOTAL_SCREENS`, the modulo operator (`%`) wraps it back to `0`. This creates a circular navigation (Screen 0 -> Screen 1 -> Screen 2 -> Screen 0, and so on).
*   **Triggering Display Update:** After changing `current_screen`, you'll likely need to signal your `display_manager.c` (which controls LVGL) to actually redraw the display to reflect the new screen. This could be by setting a flag, calling a specific function in `display_manager.c`, or sending a message via a FreeRTOS queue if you're using tasks.

**Further Considerations for HID Input:**

*   **Key Release Events:** Most HID libraries also report key release events. For some actions, you might only care about the press, but for others (like continuous scrolling while a key is held), you'd need to handle both press and release.
*   **Debouncing:** Physical buttons often require debouncing to prevent a single mechanical press from being registered as multiple presses. While USB keyboards internally handle this for their keys, if you were using simple GPIO buttons, debouncing would be a software task for `hid_handler.c`.
*   **Complexity of USB Host:** Implementing USB Host functionality can be more complex than many other peripheral interactions on an ESP32. Rely heavily on the documentation and examples provided with your chosen USB Host library.
*   **Resource Usage:** A USB Host stack can consume a noticeable amount of flash and RAM, so factor this into your project's resource budget.

By integrating USB keyboard input, you can make your ESP32 ECU Data Display far more interactive and versatile, allowing for easy navigation and control without needing a touchscreen or dedicated physical buttons for every function. This `hid_handler.c` module will be key to unlocking that interactivity.

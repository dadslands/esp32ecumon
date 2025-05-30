# ESP32 Real-Time ECU Data Display System

## Overview

This project implements a real-time Engine Control Unit (ECU) data display system using an ESP32 microcontroller. It can retrieve data from a vehicle's CAN bus (via OBD-II or directly from aftermarket ECUs), process it, and display key parameters like RPM, speed, temperatures, etc. The system also provides a web interface for Wi-Fi configuration and potentially for viewing data remotely. Status information is indicated via an RGB LED.

This guide provides the firmware and instructions to build and deploy this system.

## Key Features

*   **CAN Bus Interface:** Reads data from vehicle ECUs.
    *   Supports standard OBD-II PID requests (e.g., RPM, Speed, Coolant Temp).
    *   Designed to be extensible for aftermarket ECU data streams (manufacturer-specific CAN IDs).
*   **Wi-Fi Connectivity:**
    *   **AP Mode:** On first boot or if STA connection fails, starts an Access Point (SSID: `ESP32_ECU_Config`) for Wi-Fi configuration.
    *   **STA Mode:** Connects to a configured Wi-Fi network for potential remote data access or OTA updates.
    *   **Web Configuration Page:** Served in AP mode for easy Wi-Fi setup (SSID scanning, password entry). Credentials are saved to NVS.
*   **Data Display:**
    *   **Web Interface (STA Mode):** Placeholder for displaying real-time data (currently shows RPM).
    *   **TFT LCD Display (GC9A01):** Direct visual output of key parameters like IP address and RPM. (Phase 1 integration - basic info). LVGL can be integrated for richer UIs.
*   **RGB LED Status Indicator (WS2812B):** Provides visual feedback on system status:
    *   Booting (White)
    *   AP Mode (Pulsing Blue)
    *   STA Connecting (Blinking Yellow)
    *   STA Connected (Solid Green for 3s, then Off/Idle)
    *   STA Connection Failed (Solid Red)
    *   High RPM Warning (Flashing Red)
*   **Modular Design:** Uses ESP-IDF components for clear separation of concerns (CAN, Wi-Fi, HTTP, Display, LED).
*   **Real-Time Performance:** Utilizes FreeRTOS for concurrent task management with priority assignments for time-critical operations.

## Hardware Requirements

*   **Microcontroller:** ESP32 (ESP32-S3-N8R2 specifically mentioned, but other ESP32s with sufficient pins and PSRAM (if needed for larger displays/LVGL) should work).
*   **CAN Transceiver:** TJA1050 (or compatible, e.g., MCP2551, SN65HVD230).
    *   Connected to ESP32's TWAI pins:
        *   CAN TX: GPIO 21
        *   CAN RX: GPIO 22
*   **TFT LCD Display (Optional):** GC9A01 Round Display (240x240 resolution).
    *   SPI Interface Pins:
        *   SCLK: GPIO 14
        *   MOSI (SDA): GPIO 15
        *   CS (Chip Select): GPIO 5
        *   DC (Data/Command): GPIO 4
        *   RST (Reset): GPIO 12
        *   MISO: Not typically used for display-only.
        *   Backlight (BCKL/BLK): Optional, GPIO if controlled (e.g., GPIO 2 for this project if used, otherwise tie to 3.3V). The `bb_spi_lcd` driver is configured with `LCD_PIN_NUM_BCKL = -1` meaning it's not controlled by the library in the current setup.
*   **RGB LED:** WS2812B NeoPixel (single LED).
    *   Data Pin: GPIO 48
*   **OBD-II Connector:** To interface with the vehicle's OBD-II port.
*   **Power Supply:** Appropriate 3.3V power supply for the ESP32 and components. For in-car use, a 12V to 3.3V/5V DC-DC converter is recommended.
*   **Basic Prototyping Gear:** Breadboard, jumper wires, USB cable.

## Software Setup

*   **ESP-IDF Version:** Developed and tested with ESP-IDF v5.x (e.g., v5.1 or later recommended). Functionality relies on ESP-IDF components and drivers.
    *   Ensure ESP-IDF is installed and configured in your environment. Refer to the [official Espressif ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html).
*   **Toolchain:** Xtensa or RISC-V toolchain, depending on your ESP32 variant, as provided by ESP-IDF.

## Build and Flash Instructions

1.  **Clone the Repository (Conceptual):**
    ```bash
    # git clone <repository_url>
    # cd esp32_ecu_display
    ```

2.  **Set Target ESP32 Chip (Important!):**
    Open an ESP-IDF command prompt or terminal where the ESP-IDF environment is sourced.
    ```bash
    idf.py set-target esp32s3
    # Or your specific ESP32 target (e.g., esp32, esp32c3)
    ```

3.  **Configure (Optional, for advanced settings):**
    ```bash
    idf.py menuconfig
    # Review component settings, Wi-Fi options, etc. Default configuration should work.
    ```

4.  **Build the Project:**
    ```bash
    idf.py build
    ```

5.  **Flash the Firmware:**
    Connect your ESP32 board via USB. Ensure the correct COM port is detected.
    ```bash
    idf.py -p /dev/ttyUSB0 flash monitor
    # Replace /dev/ttyUSB0 with your ESP32's serial port (e.g., COM3 on Windows)
    ```
    The `monitor` target will display serial output from the ESP32.

## Initial Wi-Fi Setup

1.  **First Boot / No Credentials:** On the first boot, or if the ESP32 cannot connect to a previously saved Wi-Fi network, it will start in Access Point (AP) mode.
2.  **Connect to AP:** Using a phone or computer, scan for Wi-Fi networks. Connect to the network with the SSID: **`ESP32_ECU_Config`**. This is an open network.
3.  **Access Configuration Page:** Once connected, open a web browser and navigate to `http://192.168.4.1` (the default IP address for ESP32's AP mode).
4.  **Scan and Configure:**
    *   The page will load. Click the "Scan for Wi-Fi Networks" button.
    *   A list of available networks will appear in the dropdown. Select your desired network. Its name will automatically fill the SSID input field.
    *   Enter the password for your selected network.
    *   Click "Connect".
5.  **Connection Process:** The ESP32 will save these credentials to its Non-Volatile Storage (NVS) and then attempt to restart and connect to your Wi-Fi network in Station (STA) mode.
    *   The RGB LED will indicate the status (Blinking Yellow while connecting, Solid Green then Off if successful, Solid Red if failed).
    *   If the connection is successful, the ESP32 will obtain an IP address from your network. You can see this IP address on the serial monitor.
    *   If the connection fails, the ESP32 will revert to AP mode, allowing you to try configuring again.

## Accessing the Web Interface (STA Mode)

Once the ESP32 is connected to your Wi-Fi network (STA mode):
1.  Find the IP address of the ESP32 (check serial monitor output during boot, or your router's DHCP client list).
2.  Open a web browser on a device connected to the same network and navigate to `http://<ESP32_IP_ADDRESS>/`.
3.  You should see the data display page (currently showing RPM, with other data planned).

## Project Structure Overview

The project is organized into the `main` application and several custom components located in the `components/` directory:

*   **`main/`**:
    *   `app_main.c`: Main application entry point, initializes system services and tasks.
    *   `wifi_manager.c/h`: Handles Wi-Fi AP/STA modes, NVS credential storage, and Wi-Fi scanning.
    *   `http_server_handler.c/h`: Manages the web server for configuration (AP mode) and data display (STA mode).
*   **`components/`**:
    *   **`bb_spi_lcd_driver/`**: Contains the (currently full, previously stubbed) `bb_spi_lcd` library for GC9A01 display communication.
    *   **`can_reader/`**: Manages CAN bus communication (TWAI initialization, message sending/receiving, RPM parsing).
    *   **`display_manager/`**: Initializes the display hardware and runs a task to update the TFT screen with data.
    *   **`hid_handler/`**: Manages Human Interface Device interactions, currently focused on RGB LED status indication.
    *   **`data_processor/`** (Placeholder): Intended for intermediate data processing, filtering, or calculations.

This structure promotes modularity and separation of concerns.

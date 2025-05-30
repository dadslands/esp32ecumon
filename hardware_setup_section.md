## Part 2: Hardware Setup and Assembly

This is where the magic begins to take physical form! In this part, we'll gather all the necessary components and then carefully wire them together. Don't worry if you're new to electronics; we'll break it down step-by-step.

### 2.1 Components Needed (Adapting from Outline Chapter 3)

Before we can build our ECU data display, we need to collect our building blocks. Here’s a list of the core electronic components you’ll need. We've focused on common, easy-to-find parts.

1.  **ESP32 Development Board:**
    *   **Recommendation:** An ESP32-WROOM-32 based development board (often called "ESP32 DevKitC"). These usually come with a USB port for programming and power, Wi-Fi/Bluetooth, and enough pins for our project.
    *   **Why:** It's powerful, has a built-in CAN controller (called TWAI), and is widely supported. The USB port allows for easy programming and can power the device during development. Some boards or custom setups might offer a secondary USB connection for dedicated in-car power or data logging, aligning with our "dual USB support" concept for flexibility.
    *   **Key Pins to Identify:** Later, we'll need to find its 3.3V, GND (Ground), TWAI (CAN) TX/RX pins, and SPI pins (MOSI, MISO, SCK, CS).

2.  **CAN Transceiver Module:**
    *   **Recommendation:** A module based on the **SN65HVD230** chip. These are 3.3V compatible, work directly with the ESP32's TWAI controller, and are readily available.
    *   **Why:** The ESP32's CAN pins operate at 3.3V logic levels, but a car's CAN bus uses different voltage levels. This module translates between them, protecting your ESP32 and ensuring reliable communication. It typically has pins for CAN_H (CAN High), CAN_L (CAN Low), VCC (power), and GND (ground), as well as TX/RX pins to connect to the ESP32.

3.  **Display Module (SPI Interface for Prioritized Graphics):**
    *   **Recommendation:** A 1.8-inch TFT LCD display with an **ST7735 or ILI9341 controller** using the SPI interface. These offer color and enough resolution (e.g., 128x160 or 240x320 pixels) for displaying data with our desired "prioritized graphics."
    *   **Why SPI?** SPI (Serial Peripheral Interface) is faster than I2C, which is beneficial for smoother graphics and faster data updates on the display.
    *   **Key Pins:** VCC, GND, SCK/CLK (Serial Clock), MOSI/SDA/DIN (Master Out Slave In / Data In), CS/CE (Chip Select), DC/RS (Data/Command or Register Select), and sometimes RST (Reset) and LED/BLK (Backlight Control).

4.  **Addressable RGB LEDs (Neopixel / WS2812B):**
    *   **Recommendation:** A small strip or a few individual WS2812B LEDs (often called "Neopixels"). These are easy to control with a single data pin from the ESP32.
    *   **Why:** To provide visual feedback (status, alerts) as discussed in our project features.
    *   **Key Pins:** VCC/5V, GND, DIN (Data In). Some strips might have DOUT (Data Out) to chain more LEDs.

5.  **OBD-II Connector:**
    *   **Recommendation:** A male J1962 OBD-II connector with pigtail wires or a breakout board. This allows you to easily access the necessary pins from your car's OBD-II port.
    *   **Key Wires/Pins Needed:** CAN_H (usually Pin 6), CAN_L (usually Pin 14), Vehicle Ground (Pin 4 or 5), and Vehicle Power (+12V, Pin 16).

6.  **Breadboard and Jumper Wires:**
    *   **Recommendation:** A standard solderless breadboard (e.g., 830-point) and a good assortment of male-to-male (M-M) and male-to-female (M-F) jumper wires.
    *   **Why:** For easily connecting and disconnecting components during prototyping without soldering.

7.  **Power Supply Components (for in-car use):**
    *   **Recommendation:** A DC-DC buck (step-down) converter module (e.g., based on LM2596) to convert your car's 12V supply to a stable 5V or 3.3V for the ESP32 and other components.
    *   **Why:** Directly connecting to the car's 12V can damage your 3.3V/5V electronics.

**Optional but Recommended Tools:**
*   Multimeter: For checking connections and voltages.
*   Soldering iron and solder: If you want to make your project more permanent later.

### 2.2 Wiring Guide (Adapting from Outline Chapter 4)

**Safety First!**
*   **Disconnect Power:** Always disconnect the ESP32 from USB and any other power source before making wiring changes.
*   **Car Safety:** When connecting to your car, ensure the ignition is OFF. Double-check your OBD-II connections before powering anything up. Incorrect wiring can damage your car or your project!
*   **Pin Identification:** ESP32 boards vary. Always consult the pinout diagram for *your specific ESP32 board* to correctly identify GPIO numbers and functions. The pin numbers mentioned below are common defaults but verify them!

**A. Connecting the CAN Transceiver to the ESP32**

We'll use the ESP32's built-in TWAI (CAN) controller.

*   **ESP32 Pin (Example GPIOs - Verify Yours!)**  -> **SN65HVD230 Module Pin**
    *   `GPIO5` (Default ESP32 CAN_TX)             -> `TXD` or `TX` or `CTXD`
    *   `GPIO4` (Default ESP32 CAN_RX)             -> `RXD` or `RX` or `CRXD`
    *   `3.3V`                                     -> `VCC` or `3V3`
    *   `GND` (Ground)                             -> `GND`

*Placeholder for ESP32 to CAN Transceiver Wiring Diagram*

**B. Connecting the SPI TFT Display to the ESP32**

SPI displays require a few more connections than I2C ones, but offer better performance for graphics.

*   **ESP32 Pin (Example SPI Pins - Verify Yours!)** -> **SPI TFT Display Pin**
    *   `GPIO23` (Default VSPI MOSI)              -> `MOSI`, `SDA`, or `DIN`
    *   `GPIO18` (Default VSPI SCK)               -> `SCK` or `CLK`
    *   `GPIO5` (VSPI CS - *Choose an available GPIO if GPIO5 is used for CAN_TX, e.g., GPIO15*) -> `CS` or `CE` (Chip Select)
    *   `GPIO2`  (*Choose an available GPIO*)       -> `DC`, `D/C`, or `RS` (Data/Command)
    *   `GPIO4`  (*Choose an available GPIO if GPIO4 is used for CAN_RX, e.g., GPIO0 or tie to ESP32 RST via resistor*) -> `RST` or `RESET` (Reset) - *Can sometimes be tied to ESP32's EN/RST pin or 3.3V if not actively controlled.*
    *   `3.3V`                                     -> `VCC` or `VIN` or `3.3V`
    *   `GND`                                      -> `GND`
    *   `3.3V` (or dedicated GPIO for PWM control) -> `LED`, `LEDA`, or `BLK` (Backlight Anode) - *Some displays have a built-in resistor; others might need one if connecting LED pin directly to 3.3V.*

*Note on SPI Pins:* The ESP32 has multiple SPI controllers (VSPI, HSPI). VSPI is commonly used. Ensure library configurations match the pins you use. If default SPI pins (like GPIO5 for VSPI CS) are used by another peripheral (like CAN TX), you **must** choose different, available GPIOs for those functions and configure your software accordingly.

*Placeholder for ESP32 to SPI TFT Display Wiring Diagram*

**C. Connecting the Addressable RGB LEDs (WS2812B/Neopixel)**

These are surprisingly simple to wire up.

*   **ESP32 Pin (Choose an available GPIO)** -> **WS2812B LED Pin**
    *   `GPIO16` (*Example - any digital output pin*) -> `DIN` (Data Input)
    *   `5V` (If ESP32 board provides 5V from USB and LEDs need 5V) OR `3.3V` (if using few LEDs and they work with 3.3V logic/power) -> `VCC` or `5V`
    *   `GND`                                      -> `GND`

*Powering RGB LEDs:* WS2812B LEDs technically prefer 5V logic for data, but often work with the ESP32's 3.3V data signal, especially for short wire runs. For power, they draw a significant amount of current (up to 60mA per LED at full brightness white).
    *   For 1-3 LEDs, you might get away with powering them from the ESP32's 3.3V or 5V pin (if available and your USB source is robust).
    *   For more LEDs, an external 5V power supply is **highly recommended** to avoid overloading the ESP32. Ensure all grounds (ESP32 GND, external supply GND) are connected.
    *   A level shifter on the data line might be needed if you encounter issues with 5V powered LEDs not responding to the ESP32's 3.3V data signal.

*Placeholder for ESP32 to RGB LED Wiring Diagram*

**D. Connecting to the OBD-II Port**

This is the connection to your vehicle. **Proceed with extreme caution.**

1.  **Identify Wires on your OBD-II Connector:**
    *   `CAN_H`: Usually connected to Pin 6 of the OBD-II port.
    *   `CAN_L`: Usually connected to Pin 14 of the OBD-II port.
    *   `Vehicle Ground`: Usually Pin 4 (Chassis Ground) or Pin 5 (Signal Ground).
    *   `Vehicle Power (+12V)`: Usually Pin 16.

2.  **Connecting the CAN Transceiver to OBD-II Wires:**
    *   SN65HVD230 Module `CANH` -> OBD-II Connector `CAN_H` wire
    *   SN65HVD230 Module `CANL` -> OBD-II Connector `CAN_L` wire

3.  **Powering the System In-Car (Using DC-DC Buck Converter):**
    *   OBD-II `Vehicle Power (+12V)` wire -> DC-DC Buck Converter `VIN+`
    *   OBD-II `Vehicle Ground` wire        -> DC-DC Buck Converter `VIN-`
    *   DC-DC Buck Converter `VOUT+` (set to 5V or 3.3V) -> ESP32 `VIN` (if 5V) or `3.3V` pin (if 3.3V output)
    *   DC-DC Buck Converter `VOUT-`        -> ESP32 `GND`

    *Important: Ensure your DC-DC converter is correctly adjusted to the desired output voltage (e.g., 5V if your ESP32 board has its own 3.3V regulator, or 3.3V if powering the ESP32 directly on its 3.3V rail) BEFORE connecting it to the ESP32.*

*Placeholder for OBD-II and Power Supply Wiring Diagram*

**Initial Hardware Checks (Before Full Software):**
*   Once wired (bench setup first!), power on the ESP32 via USB.
*   Load a simple "Blink" sketch to confirm the ESP32 is working.
*   Load a display test sketch from the display library examples to verify display wiring.
*   Load a Neopixel strand test sketch to check RGB LED wiring.

This detailed wiring forms the backbone of our project. In the next sections, we'll breathe life into this hardware with software! Remember to take your time, double-check every connection, and consult your specific component datasheets and ESP32 board pinout diagrams.

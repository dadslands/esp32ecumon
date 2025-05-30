## Part 8: Testing & Validation - Ensuring Accuracy and Speed

Building your ESP32 ECU Data Display is a significant achievement, but how do you ensure it's working correctly and performing well? Testing and validation are crucial steps to build confidence in your device. This section will guide you through strategies to simulate CAN data for robust off-vehicle testing and methods to benchmark the critical response time from reading a CAN message to updating your display, aiming for our goal of **under 50ms latency**.

**Why is Rigorous Testing So Important?**

*   **Accuracy:** You need to trust the data displayed. Incorrect decoding or missed messages could lead to misinterpretations of your vehicle's status.
*   **Performance:** For a "real-time" display, data needs to flow from the ECU to your screen quickly. Sluggish updates make the display less useful.
*   **Reliability:** Testing helps uncover bugs or hardware issues that could cause the system to crash or behave erratically, especially important for a device used in a dynamic environment like a car.
*   **Safety (Indirectly):** While our project is primarily for display, thorough testing ensures it doesn't negatively impact other vehicle systems (though direct interference is already cautioned against).

### 8.1 Simulating CAN Data: Testing Without the Car

Continuously needing access to your car for development and testing can be impractical. Simulating CAN data allows you to test your ESP32 project conveniently on your workbench.

**Why Simulate?**

*   **Convenience:** Test anytime, anywhere, without needing to be in your car.
*   **Controlled Environment:** You control exactly what data is sent, making it easier to test specific PIDs, data ranges, or error conditions.
*   **Repeatability:** Ensure tests are consistent, which is vital for debugging.
*   **Safety:** Avoid any risk to your vehicle while debugging initial software bugs.

**Methods for CAN Data Simulation:**

1.  **Professional CAN Simulators (The High-End Option):**
    *   **Examples:** Vector CANoe/CANalyzer, Peak CAN-System PCAN-Explorer, Intrepid Control Systems Vehicle Spy.
    *   **Capabilities:** These are powerful (and often expensive) commercial tools that offer comprehensive CAN simulation, traffic generation, network analysis, and scripting. You can define complex scenarios, simulate entire ECUs, and log/analyze all bus traffic with high precision.
    *   **For Hobbyists:** While likely overkill for many hobbyist projects due to cost, it's good to be aware they exist as they represent the industry standard for automotive network development and testing.

2.  **Arduino-Based CAN Data Generator (The Accessible DIY Option):**
    *   **Concept:** Use a second microcontroller board (like another ESP32, an Arduino Uno, or STM32) equipped with its own CAN transceiver to act as a CAN message generator. You program this second board to send specific CAN messages that your main ESP32 project will receive and process.
    *   **Hardware:**
        *   An Arduino-compatible board (e.g., ESP32, Arduino Uno/Nano/Mega, STM32 Nucleo/BluePill).
        *   A CAN transceiver module (e.g., MCP2515-based for Arduino Uno, or another SN65HVD230 for an ESP32) compatible with your chosen generator board.
        *   Wiring to connect the two CAN transceivers (CAN_H to CAN_H, CAN_L to CAN_L). Remember bus termination if you're creating a small, isolated bus (a single 120-ohm resistor between CAN_H and CAN_L on this test bus is usually sufficient).
    *   **Software (Conceptual for the Generator):**
        You'll write a simple sketch for the generator board to send CAN messages that mimic OBD-II responses.

        ```cpp
        // --- Conceptual Arduino Code for a CAN Data Generator (e.g., using MCP2515 library) ---
        #include <mcp_can.h>
        #include <SPI.h>

        // Define the CAN ID your main ESP32 project expects for ECU responses
        const int ECU_RESPONSE_ID = 0x7E8;

        // Define some PIDs you want to simulate
        const byte PID_RPM = 0x0C;
        const byte PID_SPEED = 0x0D;

        MCP_CAN CAN0(10); // Set CS pin for MCP2515 (e.g., D10 on Arduino Uno)

        void setup() {
            Serial.begin(115200);
            if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) { // Adjust speed to match your project
                Serial.println("CAN Generator Initialized!");
            } else {
                Serial.println("Error Initializing CAN Generator...");
                while (1);
            }
        }

        void loop() {
            // Simulate RPM Data (e.g., 2500 RPM)
            // Formula: RPM = ((A * 256) + B) / 4  =>  (A*256)+B = RPM*4
            // 2500 RPM * 4 = 10000.  10000 = 0x2710. So, A=0x27, B=0x10
            byte rpmData[8] = {0x04, 0x41, PID_RPM, 0x27, 0x10, 0x00, 0x00, 0x00}; // Mode 01 Resp, PID, A, B
            CAN0.sendMsgBuf(ECU_RESPONSE_ID, 0, 8, rpmData);
            Serial.println("Sent RPM data");
            delay(1000); // Send every second

            // Simulate Speed Data (e.g., 60 km/h)
            // Formula: Speed = A. So A = 60 (0x3C)
            byte speedData[8] = {0x03, 0x41, PID_SPEED, 0x3C, 0x00, 0x00, 0x00, 0x00}; // Mode 01 Resp, PID, A
            CAN0.sendMsgBuf(ECU_RESPONSE_ID, 0, 8, speedData);
            Serial.println("Sent Speed data");
            delay(1000);

            // Add more simulated data as needed
        }
        ```
    *   **Benefits:** Relatively inexpensive, highly customizable, and provides excellent practice.

3.  **Pre-recorded CAN Logs (Advanced):**
    *   If you have access to tools that can capture CAN traffic from a real vehicle (like some advanced OBD-II scanners or the professional tools mentioned above), you can record logs of CAN messages.
    *   Some tools then offer the capability to "replay" these logs onto a CAN bus, which your ESP32 can listen to. This provides very realistic test data but requires more specialized equipment for capture and replay.

### 8.2 Benchmarking Response Time (CAN Read to Display Update)

A key performance indicator for our project is **latency**: the time it takes from receiving a CAN message containing vehicle data to that data being visibly updated on the display. We're aiming for a target of **under 50 milliseconds (ms)** for this entire process to ensure a "real-time" feel.

**Methodology for Measuring Latency:**

Measuring this accurately can be tricky, as it spans hardware events, software processing, and display refresh cycles.

1.  **Using an Oscilloscope or Logic Analyzer (Preferred Method):**
    This is the most accurate way to measure the true latency.
    *   **Step 1: Identify Trigger Points in Your Code.** You'll need to toggle GPIO pins at specific moments:
        *   **Trigger Point 1 (CAN Read Complete):** In your `can_reader.c` (or the task handling it), immediately after a targeted CAN message (e.g., an RPM response on ID 0x7E8) has been successfully received and its critical data bytes (e.g., bytes A and B for RPM) have been extracted.
            ```c
            // Inside can_reader.c, after successfully receiving and validating a target CAN message
            // twai_message_t rx_message;
            // ... (code to receive message into rx_message) ...
            // if (is_rpm_message(&rx_message)) {
            //     uint8_t byte_A = rx_message.data[3];
            //     uint8_t byte_B = rx_message.data[4];
            //     gpio_set_level(CAN_READ_GPIO_PIN, 1); // << TRIGGER POINT 1 HIGH
            //     // ... further processing / decoding ...
            //     gpio_set_level(CAN_READ_GPIO_PIN, 0); // << TRIGGER POINT 1 LOW (optional, creates a pulse)
            //     // Store or queue the decoded RPM value for the display task
            // }
            ```
        *   **Trigger Point 2 (Display Update Complete):** This is slightly more complex due to how LVGL and display drivers work. The ideal point is when the pixels have actually been written to the display RAM or the flush operation has completed.
            *   Inside your LVGL `flush_cb` function (e.g., `disp_driver_flush_cb` in `display_manager.c`), after the function that sends data to the display hardware via SPI has been called and *before* `lv_disp_flush_ready()` is called. If your SPI transfer is blocking, place it right after the transfer. If it's DMA-based, place it in the DMA transfer complete callback, if available, or as close as possible.
            ```c
            // Inside display_manager.c, your LVGL flush callback
            // static void disp_driver_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
            //     // ... your code to send color_p data to display hardware ...
            //     my_spi_transfer_function(area, color_p);
            //     gpio_set_level(DISPLAY_UPDATE_GPIO_PIN, 1); // << TRIGGER POINT 2 HIGH
            //     gpio_set_level(DISPLAY_UPDATE_GPIO_PIN, 0); // << TRIGGER POINT 2 LOW (optional)
            //     lv_disp_flush_ready(disp_drv);
            // }
            ```
    *   **Step 2: Connect GPIOs to Oscilloscope/Logic Analyzer.** Connect probes to the two GPIO pins you've chosen.
    *   **Step 3: Measure Time Difference.** Configure your oscilloscope/logic analyzer to trigger on the first GPIO signal (CAN Read) and measure the time until the second GPIO signal (Display Update) occurs. This time difference is your latency.

2.  **Using ESP32's High-Resolution Timer (Software Method - Less Precise for UI):**
    You can use `esp_timer_get_time()` (which returns time in microseconds) to get timestamps at different points in your code.
    *   **Timestamp 1:** In `can_reader.c`, after receiving and validating the target CAN message:
        `int64_t can_read_time = esp_timer_get_time();`
    *   **Timestamp 2:** In `display_manager.c`, after you've updated the relevant LVGL object (e.g., `lv_label_set_text()`) with the new data from the CAN task and LVGL has processed its handlers (typically after `lv_timer_handler()` runs, which updates the display based on invalidated objects).
        `int64_t display_updated_time = esp_timer_get_time();`
    *   **Calculate Latency:** `int64_t latency_us = display_updated_time - can_read_time;`
    *   **Limitations:**
        *   This measures the time until the software *thinks* the display is updated, not necessarily when pixels physically change.
        *   The timing of `lv_timer_handler()` and LVGL's internal rendering loop adds complexity.
        *   The measurement code itself adds slight overhead.
        *   It's harder to pinpoint the exact moment of visual change compared to observing a GPIO toggle aligned with the display flush.

**Tips for Achieving the < 50ms Target:**

*   **Efficient LVGL Practices:** As discussed in Part 5 (Display Optimization), use double buffering, optimize UI elements, and pre-render assets.
*   **Fast SPI Clock for Display:** Configure the SPI frequency for your TFT display to be as high as reliably supported by the display and ESP32 (often 20MHz, 40MHz, or even 80MHz for some displays/configurations).
*   **Optimized CAN Data Handling:**
    *   Process incoming CAN messages quickly in `can_reader.c`.
    *   Use efficient decoding formulas. Avoid floating-point math in ISRs if possible (though OBD-II decoding often requires it; ensure it's done in a task).
    *   Pass data to the display task efficiently (e.g., using FreeRTOS queues).
*   **Correct FreeRTOS Task Priorities:** Ensure your `can_reader_task` has the highest priority, followed by the `display_update_task`, as detailed in Part 7 (Real-Time Performance Tips). This prevents less critical tasks from delaying data acquisition or display rendering.
*   **Minimize Full Screen Refreshes:** Design your LVGL UI and use `lv_disp_drv_t` settings so that only changed portions of the screen are redrawn and flushed whenever possible (`full_refresh = 0`).

### 8.3 Practical Validation Steps: An Iterative Approach

1.  **Unit Testing (Individual Modules):**
    *   **`can_reader.c`:** Use your CAN simulator to send known CAN messages. In `can_reader.c`, print the decoded values to the serial monitor and verify they match expected results for various PIDs and data inputs. Test edge cases.
    *   **`display_manager.c`:** Write test functions that directly set values for LVGL objects (labels, gauges) and verify they appear correctly on the display, independent of CAN data. Test different graphical elements.
    *   **`hid_handler.c`:** Test keyboard inputs by printing actions to the serial monitor. Verify RGB LED control.

2.  **Integration Testing (Modules Working Together):**
    *   Combine `can_reader.c` and `display_manager.c`. Use the CAN simulator to send data. Verify that this data is correctly decoded and then displayed by LVGL. Start with one or two parameters, then gradually increase.
    *   Integrate `hid_handler.c`. Verify that keyboard inputs correctly control the display (e.g., switching screens showing simulated CAN data).

3.  **System Testing (Benchmarking and In-Vehicle):**
    *   Perform latency benchmarking as described in section 8.2 using simulated data first.
    *   Once bench testing is satisfactory, move to in-vehicle testing. Start with the ignition on (engine off if your car provides CAN data in this state) and then with the engine running.
    *   Compare the data on your ESP32 display with your car's dashboard or a trusted commercial OBD-II scanner to validate accuracy in a real environment.
    *   Look for any unexpected behavior, display glitches, or unresponsiveness under real-world conditions.

Thorough testing and validation, especially with simulated data and careful benchmarking, will give you confidence that your ESP32 ECU Data Display is not only functional but also accurate and performant. Remember that testing is often an iterative process of finding issues and refining your design.

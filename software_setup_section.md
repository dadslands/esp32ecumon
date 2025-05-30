## Part 3: Software Setup and Structure

With our hardware ready, it's time to dive into the software that will bring our ECU Data Display to life. This section will guide you through setting up the necessary development tools and understanding the basic structure of the code we'll be writing. We'll be using the Espressif IoT Development Framework (ESP-IDF), which is the official development framework for the ESP32, offering powerful tools and libraries.

### 3.1 Installing Tools: ESP-IDF, Graphics, and CAN Support

While the Arduino IDE is an option for ESP32 development (as mentioned in the full outline), for this project, we'll focus on ESP-IDF to leverage its full potential, especially for features like advanced graphics with LVGL and direct access to drivers like the CAN (TWAI) controller.

**3.1.1 ESP-IDF (Espressif IoT Development Framework)**

ESP-IDF is a comprehensive framework for developing applications on the ESP32. It includes a toolchain (compiler, linker, etc.), libraries (for Wi-Fi, Bluetooth, peripherals), and build tools.

**Step-by-Step Installation (Conceptual - Refer to Official Espressif Docs for Detail):**

Installing ESP-IDF involves a few key steps. **It's crucial to follow the latest official documentation from Espressif for your specific operating system (Windows, macOS, or Linux) as commands and procedures can be updated.** You can find this on the Espressif website by searching for "ESP-IDF Programming Guide."

Here's a general overview of the process:

1.  **Prerequisites:**
    *   **Python:** ESP-IDF build system uses Python.
    *   **Git:** For cloning ESP-IDF from its GitHub repository.
    *   **Cross-Compiler:** A compiler specific to the ESP32's Xtensa LX6 architecture.

2.  **ESP-IDF Tools Installer (Recommended for Windows):**
    *   Espressif provides an "ESP-IDF Tools Installer" for Windows that simplifies the setup of the toolchain, Git, Python, and sets up environment variables. Download and run this installer, following its on-screen instructions.

3.  **Manual Installation (Linux/macOS and advanced Windows users):**
    *   **Install Dependencies:** Use your system's package manager to install Python3, git, and any other required packages (like `python3-pip`, `python3-venv`, `cmake`, `ninja`). The official guide lists specific commands.
        *   Example (Linux): `sudo apt-get install git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0`
    *   **Get ESP-IDF:** Clone the ESP-IDF repository from GitHub. It's recommended to clone a specific release version for stability.
        ```bash
        mkdir -p ~/esp
        cd ~/esp
        git clone --recursive https://github.com/espressif/esp-idf.git
        # Or, to get a specific version (e.g., v5.1):
        # git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git
        ```
    *   **Install Tools:** Navigate into the `esp-idf` directory and run the install script.
        ```bash
        cd ~/esp/esp-idf
        ./install.sh esp32 # Specify target chip, e.g., esp32, esp32s3
        ```
    *   **Set up Environment Variables:** Each time you open a new terminal to work on an ESP-IDF project, you need to set up the environment variables. This is done by sourcing the `export.sh` script.
        ```bash
        . $HOME/esp/esp-idf/export.sh
        ```
        You can add this line to your shell's profile script (e.g., `.bashrc`, `.zshrc`) for convenience, but be aware it might affect other development environments.

4.  **Verification:**
    *   After installation, try building a sample project from the `examples` directory within ESP-IDF (e.g., `get-started/hello_world`) to ensure everything is set up correctly.
        ```bash
        cd ~/esp/esp-idf/examples/get-started/hello_world
        idf.py set-target esp32 # Set your specific ESP32 chip
        idf.py build
        ```

**3.1.2 LVGL (Light and Versatile Graphics Library)**

To create an appealing user interface with "prioritized graphics" on our TFT display, we'll use LVGL. It's a free, open-source graphics library designed for embedded systems.

*   **Integration with ESP-IDF:** LVGL is typically added as a "component" to your ESP-IDF project. Many ESP32 display driver libraries (like `esp_lcd_touch_gt911` or general TFT drivers for ESP-IDF) often include LVGL integration examples or are designed to work alongside it.
*   **Getting LVGL:** You can add LVGL to your project by:
    1.  **Using the ESP-IDF Component Registry:** Many common libraries, including LVGL and drivers, can be added using `idf.py add-dependency "lvgl/lvgl^8.3"`. Check the LVGL documentation for the recommended way to integrate with ESP-IDF.
    2.  **Git Submodule:** Adding LVGL as a Git submodule in your project's `components` directory.
        ```bash
        # In your project's root directory
        mkdir -p components
        cd components
        git submodule add https://github.com/lvgl/lvgl.git lvgl
        # You'll also need a display driver compatible with LVGL and ESP-IDF
        ```
*   **Configuration:** LVGL requires some configuration (e.g., display resolution, color depth, linking it to your specific display driver). This is usually done via an `lv_conf.h` file within your project or through ESP-IDF's `menuconfig` system if the LVGL component supports it.

**3.1.3 CAN Driver (TWAI Driver)**

The ESP32's built-in CAN controller is referred to as the TWAI (Two-Wire Automotive Interface) module. ESP-IDF provides a driver for this.

*   **Built into ESP-IDF:** The TWAI driver is part of the core ESP-IDF. You don't need to install it separately. You'll include its header file in your code (e.g., `driver/twai.h`).
*   **Configuration:** You'll configure the TWAI driver in your code by setting:
    *   GPIO pins for CAN_TX and CAN_RX.
    *   Baud rate (e.g., 500kbps, 250kbps).
    *   Acceptance filters (to specify which CAN messages your ESP32 should listen to).
*   **Usage:** The driver provides functions to initialize the controller, send CAN messages, receive CAN messages, and manage status/errors. We'll explore this in detail when we write `can_reader.c`.

**A Note on Development Environment:**
*   **Text Editor/IDE:** You'll need a good text editor or Integrated Development Environment (IDE) for writing C code. Visual Studio Code (VS Code) with the Espressif IDF Extension is highly recommended as it integrates well with ESP-IDF, providing features like code completion, debugging, and easy access to build/flash/monitor commands.

### 3.2 Code Structure: Organizing Your Project

A well-structured project is easier to understand, maintain, and debug. For our ECU Data Display, we'll modularize the code into different files, each with a specific responsibility. Here's a conceptual overview of the key C files we'll be creating in our `main` directory (or a dedicated `src` directory within an ESP-IDF project):

*   **`main.c` (or `app_main.c`):**
    *   This will be the entry point of our application.
    *   It will handle the overall initialization of the system:
        *   Initializing basic ESP32 services (like NVS if needed).
        *   Calling initialization routines for the CAN reader, display manager, and HID handler.
    *   It will contain the main application loop or manage FreeRTOS tasks that run these modules.

*   **`can_reader.c`:**
    *   **Responsibility:** All CAN bus communication logic.
    *   **Key Functions:**
        *   Initializing the ESP32's TWAI (CAN) controller with the correct pins and baud rate.
        *   Setting up acceptance filters to listen for specific OBD-II response IDs (e.g., 0x7E8-0x7EF).
        *   Sending OBD-II PID request messages to the vehicle's ECU (e.g., requests for RPM, speed, temperature to CAN ID 0x7DF).
        *   Receiving CAN messages from the bus.
        *   Parsing received OBD-II response messages to extract raw data values.
        *   Converting raw data into meaningful units (e.g., RPM values, degrees Celsius).
        *   Providing an interface (e.g., functions or queues) for other parts of the application (like `display_manager.c`) to access the decoded vehicle data.

*   **`display_manager.c`:**
    *   **Responsibility:** Managing everything related to the display and user interface.
    *   **Key Functions:**
        *   Initializing the chosen TFT display (ST7735/ILI9341) using its SPI driver.
        *   Initializing the LVGL graphics library and configuring it to work with our display driver.
        *   Creating and managing LVGL objects (labels, charts, gauges, etc.) to display data.
        *   Updating the display content with the latest data received from `can_reader.c`. This includes formatting the data for presentation and refreshing the screen.
        *   Handling different UI screens or modes if implemented (e.g., a main data screen, a settings screen).
        *   Managing the visual aspects of our "prioritized graphics."

*   **`hid_handler.c` (Human Interface Device Handler):**
    *   **Responsibility:** Managing user inputs and other feedback mechanisms like RGB LEDs.
    *   **Key Functions:**
        *   Initializing GPIOs for any buttons or rotary encoders used for user interaction (e.g., to switch display modes, change settings).
        *   Reading the state of these input devices.
        *   Debouncing input signals to prevent multiple triggers from a single press.
        *   Translating input events into actions that other modules (like `display_manager.c`) can respond to.
        *   Controlling the RGB LEDs (WS2812B/Neopixels) to provide status indications (e.g., CAN bus activity, error states, user-defined alerts) or visual effects. This includes initializing the RMT peripheral (often used for WS2812B control on ESP32) and sending color data to the LEDs.

**Header Files (`.h`):**
Each of these `.c` files will typically have a corresponding header file (e.g., `can_reader.h`, `display_manager.h`, `hid_handler.h`). These headers will declare the functions and any data structures that need to be accessible by other modules, promoting modularity and clean interfaces between components.

By organizing our code this way, we can develop and test each part of the system more independently before integrating them into the final application. This approach is standard in embedded systems development and scales well as projects grow in complexity.

Next, we'll start populating these files with the actual logic, beginning with CAN bus communication.

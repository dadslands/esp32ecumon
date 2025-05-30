# CAN Reader Component

## Overview

The `can_reader` component is responsible for all aspects of Controller Area Network (CAN) bus communication for the ESP32 ECU Data Display project. It utilizes the ESP32's built-in TWAI (Two-Wire Automotive Interface) controller.

## Key Responsibilities

*   **TWAI Driver Initialization:** Configures and initializes the TWAI peripheral with the specified GPIO pins for CAN TX and CAN RX, sets the communication bitrate (e.g., 500kbps), and configures acceptance filters to determine which CAN messages are processed.
*   **CAN Message Transmission:** Provides functions to send CAN messages, specifically for requesting data from ECUs using protocols like OBD-II (e.g., requesting Engine RPM).
*   **CAN Message Reception:** Implements logic to receive incoming CAN messages from the bus.
*   **Data Parsing & Decoding:** Includes functionality to parse received CAN messages, identify relevant data (e.g., OBD-II PID responses), and decode the raw data bytes into meaningful values (e.g., calculating RPM from specific bytes in a CAN frame).
*   **Data Storage & Retrieval:** Stores the latest decoded vehicle data (like RPM) in a thread-safe manner (using mutexes) and provides getter functions for other components (e.g., `display_manager`, `http_server_handler`) to access this data.
*   **Real-time Task:** Typically runs a FreeRTOS task (`can_reader_task`) that periodically handles message requests, reception, and processing to ensure data is kept up-to-date.

## Public API

Key public functions are declared in `can_reader.h`:

*   `esp_err_t can_reader_init(void);`
    *   Initializes the CAN reader module and TWAI driver.
*   `esp_err_t can_reader_receive(twai_message_t* message, TickType_t timeout_ticks);`
    *   Receives a CAN message.
*   `esp_err_t can_reader_request_rpm(void);`
    *   Sends an OBD-II request for RPM.
*   `float can_reader_get_latest_rpm(void);`
    *   Retrieves the most recently decoded RPM value.
*   `void can_reader_task(void *pvParameters);`
    *   The FreeRTOS task function for this component.

## Configuration

*   CAN TX Pin: GPIO 21
*   CAN RX Pin: GPIO 22
*   Bitrate: 500kbps (configurable in `can_reader.c`)
*   OBD-II Request ID: `0x7DF`
*   OBD-II Response ID (Engine ECU): `0x7E8`

This component is crucial for interfacing with the vehicle's communication bus and extracting the raw data needed by the rest of the system.

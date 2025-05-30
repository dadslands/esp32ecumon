#ifndef CAN_READER_H
#define CAN_READER_H

#include "esp_err.h"
#include "driver/twai.h" // For twai_message_t
#include "freertos/FreeRTOS.h" // For TickType_t
#include "freertos/semphr.h" // For SemaphoreHandle_t

/**
 * @brief Structure to hold vehicle data retrieved from CAN bus.
 * Currently includes RPM and a timestamp of the last update.
 */
typedef struct {
    float rpm;                  /*!< Latest engine RPM value. */
    uint32_t last_update_time;  /*!< Timestamp (ms since boot) of the last RPM update. */
    // Add other vehicle parameters here as needed, e.g.:
    // float speed_kmh;
    // float coolant_temp_c;
} vehicle_data_t;

/**
 * @brief Initializes the CAN reader module.
 *
 * Configures and installs the ESP32's TWAI (CAN) driver with predefined GPIO pins
 * for TX and RX, sets the bitrate, and configures acceptance filters (currently accept-all).
 * It also creates a mutex for thread-safe access to shared vehicle data.
 *
 * @return esp_err_t ESP_OK on successful initialization, or an error code on failure.
 */
esp_err_t can_reader_init(void);

/**
 * @brief Receives a CAN message from the TWAI driver.
 *
 * This function attempts to receive a CAN message. It's a wrapper around `twai_receive`.
 *
 * @param[out] message Pointer to a `twai_message_t` structure to store the received message.
 * @param timeout_ticks Ticks to wait for a message. Use `pdMS_TO_TICKS()` to convert from ms.
 * @return esp_err_t
 *         - ESP_OK if a message is received successfully.
 *         - ESP_ERR_TIMEOUT if no message is received within the timeout.
 *         - ESP_ERR_INVALID_ARG if `message` is NULL.
 *         - Other esp_err_t codes for different TWAI driver errors.
 */
esp_err_t can_reader_receive(twai_message_t* message, TickType_t timeout_ticks);

/**
 * @brief Sends an OBD-II request for Engine RPM (PID 0x0C) over the CAN bus.
 *
 * @return esp_err_t ESP_OK if the message was queued for transmission successfully,
 *         or an error code on failure (e.g., ESP_ERR_TIMEOUT if TX queue is full).
 */
esp_err_t can_reader_request_rpm(void);

/**
 * @brief Retrieves the latest Engine RPM value.
 *
 * This function is thread-safe, using a mutex to protect access to the stored RPM data.
 *
 * @return float The latest RPM value. Returns 0.0f if no data has been received yet or
 *               if the mutex cannot be obtained.
 */
float can_reader_get_latest_rpm(void);

/**
 * @brief FreeRTOS task for the CAN reader.
 *
 * This task periodically:
 * 1. Requests RPM data using `can_reader_request_rpm()`.
 * 2. Calls `can_reader_receive()` to check for incoming CAN messages.
 * 3. If an RPM response is identified (correct CAN ID and PID), it parses the data,
 *    calculates the RPM, and updates the shared `vehicle_data_t` structure.
 * 4. Logs received messages and parsed RPM data.
 *
 * @param pvParameters Task parameters (not used).
 */
void can_reader_task(void *pvParameters);

#endif // CAN_READER_H

#include "can_reader.h"
#include "esp_log.h"
#include "driver/gpio.h" // For direct GPIO manipulation if needed outside TWAI
#include "driver/twai.h" // Now included via can_reader.h, but good for clarity
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

static const char *TAG = "can_reader";

// Define CAN bus pins and bitrate
#define CAN_TX_GPIO GPIO_NUM_21
#define CAN_RX_GPIO GPIO_NUM_22
// Bitrate is 500kbps, using predefined ESP-IDF configuration in twai_timing_config_t

// OBD-II related definitions
#define OBDII_REQUEST_ID             0x7DFU   // Standard 11-bit ID for OBD-II requests
#define OBDII_ENGINE_ECU_RESPONSE_ID 0x7E8U   // Typical response ID from primary engine ECU
// Other ECUs might respond on 0x7E9 to 0x7EF

#define OBDII_MODE_CURRENT_DATA 0x01  // Service $01: Show current data
#define OBDII_PID_RPM           0x0C  // PID for Engine RPM

// Global storage for the latest vehicle data (RPM in this case)
static vehicle_data_t g_vehicle_data = {0.0f, 0}; // Initialize RPM to 0 and time to 0
// Mutex to protect access to g_vehicle_data
static SemaphoreHandle_t g_vehicle_data_mutex = NULL;

/**
 * @brief Initializes the CAN reader module.
 * (Public function documentation is in can_reader.h)
 */
esp_err_t can_reader_init(void) {
    ESP_LOGI(TAG, "Initializing CAN Reader module (TWAI driver)...");

    // Create mutex for thread-safe access to g_vehicle_data
    g_vehicle_data_mutex = xSemaphoreCreateMutex();
    if (g_vehicle_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create vehicle data mutex!");
        return ESP_FAIL; // Critical failure
    }

    // Configure General TWAI settings
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
    // g_config.tx_queue_len = 5; // Default is 5, can be adjusted
    // g_config.rx_queue_len = 10; // Increased RX queue for potentially bursty traffic

    // Configure TWAI timing for 500kbps
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

    // Configure TWAI Filter to accept messages.
    // For initial RPM request, we expect responses from IDs 0x7E8-0x7EF.
    // A more specific filter can be set up later if needed.
    // For now, accept-all is useful for debugging and seeing all traffic.
    twai_filter_config_t f_config = {
        .acceptance_code = 0x00000000,
        .acceptance_mask = 0xFFFFFFFF,      // Accept all CAN IDs
        .single_filter = true               // Use single filter mode
    };

    ESP_LOGI(TAG, "Installing TWAI driver...");
    esp_err_t install_err = twai_driver_install(&g_config, &t_config, &f_config);
    if (install_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver: %s", esp_err_to_name(install_err));
        vSemaphoreDelete(g_vehicle_data_mutex); // Clean up created mutex
        g_vehicle_data_mutex = NULL;
        return install_err;
    }
    ESP_LOGI(TAG, "TWAI driver installed successfully.");

    ESP_LOGI(TAG, "Starting TWAI driver...");
    esp_err_t start_err = twai_start();
    if (start_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(start_err));
        twai_driver_uninstall(); // Attempt to uninstall driver if start fails
        vSemaphoreDelete(g_vehicle_data_mutex);
        g_vehicle_data_mutex = NULL;
        return start_err;
    }
    ESP_LOGI(TAG, "TWAI driver started successfully.");

    ESP_LOGI(TAG, "CAN Reader initialized successfully.");
    return ESP_OK;
}

/**
 * @brief Retrieves the latest Engine RPM value.
 * (Public function documentation is in can_reader.h)
 */
float can_reader_get_latest_rpm(void) {
    float rpm_value = 0.0f; // Default to 0 if no data or mutex issue
    if (g_vehicle_data_mutex != NULL) {
        if (xSemaphoreTake(g_vehicle_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) { // Wait up to 100ms for mutex
            rpm_value = g_vehicle_data.rpm;
            xSemaphoreGive(g_vehicle_data_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to take vehicle_data_mutex in get_latest_rpm within 100ms.");
            // Return last known or 0, depending on desired behavior on contention
        }
    } else {
        ESP_LOGE(TAG, "RPM data mutex not initialized in get_latest_rpm!");
    }
    return rpm_value;
}

/**
 * @brief Sends an OBD-II request for Engine RPM (PID 0x0C) over the CAN bus.
 * (Public function documentation is in can_reader.h)
 */
esp_err_t can_reader_request_rpm(void) {
    twai_message_t message;
    message.identifier = OBDII_REQUEST_ID;
    message.flags = TWAI_MSG_FLAG_NONE; // Standard ID, Data frame (not RTR)
    message.data_length_code = 8;       // OBD-II requests are typically 8 bytes

    message.data[0] = 0x02; // Number of additional data bytes in this request (Mode + PID)
    message.data[1] = OBDII_MODE_CURRENT_DATA; // Service $01: Show Current Data
    message.data[2] = OBDII_PID_RPM;           // PID for Engine RPM ($0C)
    message.data[3] = 0x00; // Padding bytes, often "don't care" for requests
    message.data[4] = 0x00;
    message.data[5] = 0x00;
    message.data[6] = 0x00;
    message.data[7] = 0x00; // Or 0x55 as some sniffers show

    esp_err_t ret = twai_transmit(&message, pdMS_TO_TICKS(100)); // Wait up to 100ms to queue for transmission
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "RPM request message queued for transmission.");
    } else {
        ESP_LOGE(TAG, "Failed to queue RPM request message: %s", esp_err_to_name(ret));
        // Could log TWAI status here if transmit fails repeatedly
        // twai_status_info_t status_info;
        // if (twai_get_status_info(&status_info) == ESP_OK) {
        //     ESP_LOGE(TAG, "TWAI Status: state=%d, alerts=0x%x, tx_err_cnt=%d, rx_err_cnt=%d",
        //              status_info.state, status_info.alert_flags, status_info.tx_error_counter, status_info.rx_error_counter);
        // }
    }
    return ret;
}

/**
 * @brief Receives a CAN message from the TWAI driver.
 * (Public function documentation is in can_reader.h)
 */
esp_err_t can_reader_receive(twai_message_t* message, TickType_t timeout_ticks) {
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    // Attempt to receive a message from the TWAI driver's RX queue
    esp_err_t ret = twai_receive(message, timeout_ticks);
    // Detailed logging moved to the task to reduce noise if this function is called frequently.
    return ret;
}

/**
 * @brief FreeRTOS task for the CAN reader.
 * (Public function documentation is in can_reader.h)
 */
void can_reader_task(void *pvParameters) {
    ESP_LOGI(TAG, "CAN Reader Task started. Configuring periodic RPM requests.");
    twai_message_t rx_message;
    TickType_t last_rpm_request_time = 0;
    const TickType_t rpm_request_interval_ticks = pdMS_TO_TICKS(1000); // Request RPM every 1 second

    while (1) {
        // Periodically request RPM data
        if ((xTaskGetTickCount() - last_rpm_request_time) >= rpm_request_interval_ticks) {
            ESP_LOGD(TAG, "Requesting RPM...");
            can_reader_request_rpm();
            last_rpm_request_time = xTaskGetTickCount();
        }

        // Check for incoming CAN messages with a short timeout
        esp_err_t result = can_reader_receive(&rx_message, pdMS_TO_TICKS(100));

        if (result == ESP_OK) {
            ESP_LOGD(TAG, "CAN MSG RX: ID=0x%03lX DLC=%d Flags=0x%X",
                     (unsigned long)rx_message.identifier,
                     rx_message.data_length_code,
                     rx_message.flags);

            // Log data bytes for debugging if needed (can be very verbose)
            // #if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_VERBOSE)
            // if (rx_message.data_length_code > 0) {
            //     char data_str[rx_message.data_length_code * 3 + 1];
            //     for (int i = 0; i < rx_message.data_length_code; i++) {
            //         sprintf(&data_str[i * 3], "%02X ", rx_message.data[i]);
            //     }
            //     ESP_LOGV(TAG, "  Data: %s", data_str);
            // }
            // #endif

            // Check if this is an OBD-II response for RPM
            if ((rx_message.identifier >= OBDII_ENGINE_ECU_RESPONSE_ID && rx_message.identifier <= 0x7EFU) &&
                !(rx_message.flags & TWAI_MSG_FLAG_RTR) && // Ensure it's a data frame, not Remote Transmission Request
                rx_message.data_length_code >= 5) {       // Need at least 5 bytes for RPM response (count, mode, pid, A, B)

                // Expected OBD-II response format:
                // Byte 0: Number of data bytes that follow this one (e.g., 0x04 for RPM)
                // Byte 1: Mode response (0x41 for Mode 01 response)
                // Byte 2: PID echoed back (e.g., 0x0C for RPM)
                // Byte 3: Data Byte A (MSB for RPM)
                // Byte 4: Data Byte B (LSB for RPM)
                if (rx_message.data[1] == (OBDII_MODE_CURRENT_DATA + 0x40) &&
                    rx_message.data[2] == OBDII_PID_RPM) {

                    uint8_t byte_A = rx_message.data[3];
                    uint8_t byte_B = rx_message.data[4];
                    // RPM Formula: ((A * 256) + B) / 4
                    float current_rpm = ((byte_A * 256.0f) + byte_B) / 4.0f;

                    if (g_vehicle_data_mutex != NULL && xSemaphoreTake(g_vehicle_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        g_vehicle_data.rpm = current_rpm;
                        g_vehicle_data.last_update_time = (uint32_t)(esp_timer_get_time() / 1000); // Store time in ms
                        xSemaphoreGive(g_vehicle_data_mutex);
                        ESP_LOGI(TAG, "RPM Updated: %.0f", current_rpm);
                    } else {
                        ESP_LOGW(TAG, "Failed to take vehicle_data_mutex in can_reader_task to update RPM.");
                    }
                }
            }
        } else if (result == ESP_ERR_TIMEOUT) {
            // This is normal if the bus is quiet or filters don't match any messages.
            // No action needed, loop will continue.
        } else {
            // An actual error occurred during receive
            ESP_LOGE(TAG, "CAN receive error: %s.", esp_err_to_name(result));
            // Consider checking TWAI status for bus off or other critical errors
            // twai_status_info_t status_info;
            // if (twai_get_status_info(&status_info) == ESP_OK) {
            //     if (status_info.state == TWAI_STATE_BUS_OFF) {
            //         ESP_LOGE(TAG, "TWAI driver is BUS OFF. Attempting recovery...");
            //         twai_recover_from_bus_off(pdMS_TO_TICKS(1000)); // Example recovery attempt
            //     }
            // }
            vTaskDelay(pdMS_TO_TICKS(500)); // Wait a bit before retrying after an error
        }
        // Yield to other tasks, especially if receive timeout is very short or zero
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "wifi_manager.h"
#include "http_server_handler.h"
#include "can_reader.h"
#include "hid_handler.h"
#include "display_manager.h"

static const char *TAG = "app_main";

// Define task priorities
// Higher number means higher priority
#define LED_CONTROLLER_TASK_PRIO 3  // Low priority for LED updates
#define DISPLAY_TASK_PRIO        5  // Display updates
#define WIFI_HTTP_TASK_PRIO      6  // Wi-Fi and HTTP server tasks (if any explicit ones, or for event handling)
#define CAN_TASK_PRIO            10 // High priority for time-sensitive CAN operations

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing application...");
    BaseType_t xReturned; // For checking task creation result
    esp_err_t ret;      // For checking ESP-IDF API return values

    // Initialize NVS - Non-Volatile Storage for Wi-Fi credentials and other persistent data
    ESP_LOGI(TAG, "Initializing NVS...");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_LOGW(TAG, "NVS partition was truncated or new version found. Erasing and re-initializing NVS.");
      ESP_ERROR_CHECK(nvs_flash_erase()); // Erase NVS if not valid
      ret = nvs_flash_init();             // Retry initialization
    }
    ESP_ERROR_CHECK(ret); // Halt on critical NVS init failure
    ESP_LOGI(TAG, "NVS Initialized.");

    // Initialize default ESP-IDF event loop.
    // This allows components to register event handlers for system events (Wi-Fi, IP, etc.).
    ESP_LOGI(TAG, "Creating default event loop...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "Default event loop created.");

    // Initialize TCP/IP network interface (required for Wi-Fi)
    ESP_LOGI(TAG, "Initializing network interface...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGI(TAG, "Network interface initialized.");

    // Initialize LED Controller (part of HID handler)
    ESP_LOGI(TAG, "Initializing LED Controller...");
    ret = hid_led_controller_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED controller. Continuing without LED feedback.");
        // Non-critical failure, application can continue.
    } else {
        // Set initial LED state to BOOTING
        hid_led_controller_set_system_state(SYS_LED_STATE_BOOTING);
        // Create the LED controller task
        xReturned = xTaskCreate(hid_led_controller_task, "led_ctrl_task", 2048, NULL, LED_CONTROLLER_TASK_PRIO, NULL);
        if (xReturned != pdPASS) {
            ESP_LOGE(TAG, "Failed to create LED controller task.");
            // Non-critical, continue.
        } else {
            ESP_LOGI(TAG, "LED controller task created.");
        }
    }

    // Initialize CAN Reader
    ESP_LOGI(TAG, "Initializing CAN reader...");
    ret = can_reader_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CRITICAL: Failed to initialize CAN reader. Halting application.");
        hid_led_controller_set_system_state(SYS_LED_STATE_STA_FAILED); // Indicate critical failure via LED
        while(1) { vTaskDelay(portMAX_DELAY); } // Halt
    } else {
        // Create the CAN reader task
        xReturned = xTaskCreate(can_reader_task, "can_reader_task", 4096, NULL, CAN_TASK_PRIO, NULL);
        if (xReturned != pdPASS) {
            ESP_LOGE(TAG, "CRITICAL: Failed to create CAN reader task. Halting application.");
            hid_led_controller_set_system_state(SYS_LED_STATE_STA_FAILED); // Indicate critical failure
            while(1) { vTaskDelay(portMAX_DELAY); } // Halt
        } else {
            ESP_LOGI(TAG, "CAN reader task created.");
        }
    }

    // Initialize Display Manager
    ESP_LOGI(TAG, "Initializing Display Manager...");
    ret = display_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Display Manager. Display functionality will be unavailable.");
        // Non-critical for core operation, but visual feedback will be missing.
    } else {
        // Create the Display Manager task
        xReturned = xTaskCreate(display_manager_task, "disp_mgr_task", 4096, NULL, DISPLAY_TASK_PRIO, NULL);
        if (xReturned != pdPASS) {
            ESP_LOGE(TAG, "Failed to create Display Manager task.");
        } else {
            ESP_LOGI(TAG, "Display Manager task created.");
        }
    }

    // Initialize and start Wi-Fi manager.
    // This will also start the HTTP server based on the Wi-Fi mode (AP or STA).
    // Wi-Fi manager will set LED states accordingly.
    ESP_LOGI(TAG, "Initializing Wi-Fi Manager...");
    wifi_manager_init();

    ESP_LOGI(TAG, "Main application initialization sequence complete.");

    // Main task loop (can be used for periodic checks or can be minimal if all work is in other tasks)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGD(TAG, "Main task heartbeat. Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    }
}

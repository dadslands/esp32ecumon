#include "wifi_manager.h"
#include "http_server_handler.h"
#include "hid_handler.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "wifi_manager";

// NVS Namespace and Keys for storing Wi-Fi credentials
#define NVS_NAMESPACE "wifi_creds"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASS "password"

// Event group bits to signal Wi-Fi events
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;    // Set when STA connects to AP
const int WIFI_FAIL_BIT = BIT1;         // Set if STA fails to connect after retries
const int WIFI_AP_STARTED_BIT = BIT2;   // Set when AP mode has successfully started

static int s_retry_num = 0; // Counter for STA connection retries
#define WIFI_MAX_RETRY 5    // Maximum number of retries for STA connection

// Module-level state trackers
static bool s_is_ap_mode = false;       // True if currently in AP mode
static bool s_sta_connected = false;    // True if STA is connected and has an IP
static esp_netif_ip_info_t s_ip_info;  // Stores current IP info when STA is connected

// Forward declarations for static functions
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void start_ap_mode(void);
static void start_sta_mode(const char* ssid, const char* password);
static esp_err_t load_sta_config(char* ssid, size_t ssid_len, char* password, size_t pass_len);

/**
 * @brief Initializes the Wi-Fi manager.
 * (Public function documentation is in wifi_manager.h)
 */
void wifi_manager_init(void) {
    // Create event group only once
    if (wifi_event_group == NULL) {
        wifi_event_group = xEventGroupCreate();
    } else {
        // Clear bits if re-initializing (e.g., after mode switch)
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_AP_STARTED_BIT);
    }

    // esp_netif_init() and esp_event_loop_create_default() are called once in app_main.

    char stored_ssid[32] = {0};
    char stored_password[64] = {0};

    // Attempt to load saved STA credentials
    if (load_sta_config(stored_ssid, sizeof(stored_ssid), stored_password, sizeof(stored_password)) == ESP_OK) {
        ESP_LOGI(TAG, "Found stored credentials. SSID: '%s'. Attempting STA mode.", stored_ssid);
        start_sta_mode(stored_ssid, stored_password);
    } else {
        ESP_LOGI(TAG, "No stored credentials or error loading. Starting AP mode.");
        start_ap_mode();
    }
}

/**
 * @brief Saves Wi-Fi station (client) mode credentials to NVS.
 * (Public function documentation is in wifi_manager.h)
 */
esp_err_t wifi_manager_save_sta_config(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    ESP_LOGI(TAG, "Saving Wi-Fi credentials to NVS. SSID: %s", ssid);
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_SSID, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS: Failed to write SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_PASS, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS: Failed to write Password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS: Commit failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Wi-Fi credentials saved successfully.");
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Stops AP mode and attempts to restart Wi-Fi in Station (STA) mode.
 * (Public function documentation is in wifi_manager.h)
 */
void wifi_manager_start_sta_mode_from_ap(void) {
    ESP_LOGI(TAG, "Switching from AP to STA mode requested.");

    // Stop HTTP server if it was running for AP mode
    http_server_stop();

    // Stop and de-initialize Wi-Fi (AP mode)
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
    ESP_ERROR_CHECK(esp_wifi_deinit());

    // Destroy the AP network interface
    esp_netif_t *wifiAP = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (wifiAP) {
        esp_netif_destroy(wifiAP);
        ESP_LOGI(TAG, "Default AP netif destroyed.");
    }

    ESP_LOGI(TAG, "Wi-Fi AP resources stopped and de-initialized.");
    vTaskDelay(pdMS_TO_TICKS(500)); // Brief delay to allow resources to settle

    // Re-initialize Wi-Fi manager, which will attempt STA connection
    wifi_manager_init();
}

/**
 * @brief Performs a Wi-Fi scan for available Access Points.
 * (Public function documentation is in wifi_manager.h)
 */
int wifi_manager_scan_ssids(wifi_ap_scan_result_t *results, uint16_t max_results) {
    if (!results || max_results == 0) {
        ESP_LOGE(TAG, "Scan: Invalid arguments for results buffer or max_results.");
        return -1;
    }

    ESP_LOGI(TAG, "Starting Wi-Fi scan...");
    // Default scan configuration: active scan, all channels, no specific SSID/BSSID
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0, // Scan all channels
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    // Start blocking scan
    esp_err_t scan_err = esp_wifi_scan_start(&scan_config, true);
    if (scan_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wi-Fi scan: %s", esp_err_to_name(scan_err));
        return -1;
    }
    ESP_LOGI(TAG, "Wi-Fi scan completed.");

    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num); // Get number of APs found
    ESP_LOGI(TAG, "Found %u access points.", ap_num);

    if (ap_num == 0) {
        return 0; // No APs found
    }

    // Get records for up to max_results
    uint16_t actual_aps_to_get = MIN(ap_num, max_results);
    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * actual_aps_to_get);
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "Scan: Failed to allocate memory for AP records.");
        return -1;
    }

    esp_err_t get_records_err = esp_wifi_scan_get_ap_records(&actual_aps_to_get, ap_records);
    int found_count = -1; // Default to error
    if (get_records_err == ESP_OK) {
        ESP_LOGI(TAG, "Retrieved %u AP records.", actual_aps_to_get);
        for (uint16_t i = 0; i < actual_aps_to_get; i++) {
            strncpy(results[i].ssid, (char*)ap_records[i].ssid, sizeof(results[i].ssid) - 1);
            results[i].ssid[sizeof(results[i].ssid) - 1] = '\0'; // Ensure null termination
            results[i].rssi = ap_records[i].rssi;
            results[i].authmode = ap_records[i].authmode;
            ESP_LOGD(TAG, "AP Found: SSID='%s', RSSI=%d, AUTH=%d", results[i].ssid, results[i].rssi, results[i].authmode);
        }
        found_count = actual_aps_to_get;
    } else {
        ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(get_records_err));
    }

    free(ap_records); // Free the allocated buffer
    return found_count;
}

/**
 * @brief Retrieves the current IP address or Wi-Fi status as a string.
 * (Public function documentation is in wifi_manager.h)
 */
esp_err_t wifi_manager_get_ip_info_str(char* ip_str_buffer, size_t buffer_len) {
    if (ip_str_buffer == NULL || buffer_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_is_ap_mode) {
        esp_netif_ip_info_t ap_ip_info;
        esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"); // Default AP IF key
        if (ap_netif && esp_netif_get_ip_info(ap_netif, &ap_ip_info) == ESP_OK) {
            snprintf(ip_str_buffer, buffer_len, "AP: " IPSTR, IP2STR(&ap_ip_info.ip));
        } else {
            strncpy(ip_str_buffer, "AP Mode", buffer_len); // AP might not have an "IP" in the same sense
        }
    } else if (s_sta_connected) {
        // s_ip_info is updated by the IP_EVENT_STA_GOT_IP event handler
        snprintf(ip_str_buffer, buffer_len, IPSTR, IP2STR(&s_ip_info.ip));
    } else {
        // If STA mode was attempted but not yet connected or failed
        EventBits_t bits = xEventGroupGetBits(wifi_event_group); // Check current status
        if(bits & WIFI_FAIL_BIT) {
             strncpy(ip_str_buffer, "STA Connection Failed", buffer_len);
        } else {
             strncpy(ip_str_buffer, "STA Connecting...", buffer_len);
        }
    }
    ip_str_buffer[buffer_len - 1] = '\0'; // Ensure null termination
    return ESP_OK;
}

/**
 * @brief Checks if Wi-Fi is currently connected in STA mode.
 * (Public function documentation is in wifi_manager.h)
 */
bool wifi_manager_is_sta_connected(void) {
    return s_sta_connected;
}

// --- Private Functions ---

/**
 * @brief Unified event handler for Wi-Fi and IP events.
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                s_sta_connected = false; // Reset on new attempt
                hid_led_controller_set_system_state(SYS_LED_STATE_STA_CONNECTING);
                ESP_LOGI(TAG, "WIFI_EVENT_STA_START: Initiating connection...");
                esp_wifi_connect(); // Start connection process
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                s_sta_connected = false;
                ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED.");
                if (s_retry_num < WIFI_MAX_RETRY) {
                    esp_wifi_connect(); // Retry connection
                    s_retry_num++;
                    ESP_LOGI(TAG, "Retrying Wi-Fi connection (%d/%d)...", s_retry_num, WIFI_MAX_RETRY);
                    hid_led_controller_set_system_state(SYS_LED_STATE_STA_CONNECTING);
                } else {
                    ESP_LOGE(TAG, "Failed to connect to STA after %d retries.", WIFI_MAX_RETRY);
                    hid_led_controller_set_system_state(SYS_LED_STATE_STA_FAILED);
                    xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT); // Signal connection failure
                }
                break;
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_START: AP started successfully.");
                s_is_ap_mode = true;
                s_sta_connected = false;
                hid_led_controller_set_system_state(SYS_LED_STATE_AP_MODE);
                xEventGroupSetBits(wifi_event_group, WIFI_AP_STARTED_BIT); // Signal AP started
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP: AP stopped.");
                s_is_ap_mode = false;
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Station "MACSTR" joined AP, AID=%d", MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "Station "MACSTR" left AP, AID=%d", MAC2STR(event->mac), event->aid);
                break;
            }
            default:
                ESP_LOGD(TAG, "Unhandled WIFI_EVENT: %ld", event_id);
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP: Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_ip_info = event->ip_info; // Store IP info
        s_retry_num = 0; // Reset retry counter on successful connection
        s_sta_connected = true;
        s_is_ap_mode = false;
        hid_led_controller_set_system_state(SYS_LED_STATE_STA_CONNECTED_TEMP);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT); // Signal connection success
    } else {
        ESP_LOGD(TAG, "Unhandled event: base=%s, id=%ld", event_base, event_id);
    }
}

/**
 * @brief Configures and starts Wi-Fi in Access Point (AP) mode.
 */
static void start_ap_mode(void) {
    ESP_LOGI(TAG, "Configuring Wi-Fi in AP mode. SSID: %s", WIFI_MANAGER_CONFIG_AP_SSID);
    s_is_ap_mode = true; // Set mode flag
    s_sta_connected = false;
    // LED state will be set by WIFI_EVENT_AP_START handler

    // Destroy STA netif if it exists from a previous failed attempt
    esp_netif_t *wifiSTA = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (wifiSTA) { esp_netif_destroy(wifiSTA); ESP_LOGI(TAG, "Default STA netif destroyed for AP mode.");}

    // Create default AP network interface
    esp_netif_create_default_wifi_ap();

    // Initialize Wi-Fi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers for Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // Configure AP settings
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid_len = strlen(WIFI_MANAGER_CONFIG_AP_SSID),
            .channel = 1, // Default channel
            .max_connection = 4, // Max connected stations
            .authmode = WIFI_AUTH_OPEN // Open AP for configuration page
            // .beacon_interval = 100, // Default is 100ms
        },
    };
    strncpy((char*)wifi_ap_config.ap.ssid, WIFI_MANAGER_CONFIG_AP_SSID, sizeof(wifi_ap_config.ap.ssid)-1);
    // No password for config AP: strcpy((char*)wifi_ap_config.ap.password, "");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_ERROR_CHECK(esp_wifi_start()); // This triggers WIFI_EVENT_AP_START

    ESP_LOGI(TAG, "Wi-Fi AP configuration complete. Waiting for AP_START event to confirm.");
    // HTTP server is started once AP_START event is received and handled, or here directly.
    // For simplicity, start it here as AP mode is definitively chosen.
    http_server_start(HTTP_SERVER_MODE_AP);
}

/**
 * @brief Configures and starts Wi-Fi in Station (STA) mode, attempting to connect to a saved AP.
 */
static void start_sta_mode(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Configuring Wi-Fi in STA mode. SSID: %s", ssid);
    s_retry_num = 0;
    s_is_ap_mode = false;
    s_sta_connected = false;
    // LED state (STA_CONNECTING) will be set by WIFI_EVENT_STA_START handler

    // Destroy AP netif if it exists
    esp_netif_t *wifiAP = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (wifiAP) { esp_netif_destroy(wifiAP); ESP_LOGI(TAG, "Default AP netif destroyed for STA mode.");}

    // Create default STA network interface
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    // Instance handles are useful if needing to unregister specific handlers later,
    // but for generic handlers like this, NULL is often fine if deinit cleans them.
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_sta_config = {0};
    strncpy((char*)wifi_sta_config.sta.ssid, ssid, sizeof(wifi_sta_config.sta.ssid) -1);
    strncpy((char*)wifi_sta_config.sta.password, password, sizeof(wifi_sta_config.sta.password) -1);
    // wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; // Or determine from scan

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    ESP_ERROR_CHECK(esp_wifi_start()); // This triggers WIFI_EVENT_STA_START
    ESP_LOGI(TAG, "Wi-Fi STA mode configuration complete. Waiting for connection event...");

    // Wait for connection or failure event
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "STA Mode: Successfully connected to AP: %s", ssid);
        // LED state SYS_LED_STATE_STA_CONNECTED_TEMP already set by IP_EVENT_STA_GOT_IP handler
        http_server_start(HTTP_SERVER_MODE_STA);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "STA Mode: Failed to connect to AP: '%s'. Reverting to AP mode.", ssid);
        // LED state SYS_LED_STATE_STA_FAILED already set by WIFI_EVENT_STA_DISCONNECTED handler
        esp_wifi_stop();
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
        esp_wifi_deinit();
        esp_netif_t *wifiSTA = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (wifiSTA) { esp_netif_destroy(wifiSTA); ESP_LOGI(TAG, "Default STA netif destroyed during revert.");}
        vTaskDelay(pdMS_TO_TICKS(100));
        start_ap_mode(); // This will set LED to AP_MODE via its own event handler
    } else {
        ESP_LOGE(TAG, "STA Mode: UNEXPECTED WIFI EVENT. Reverting to AP mode.");
        start_ap_mode();
    }
}

/**
 * @brief Loads Wi-Fi station credentials from NVS.
 */
static esp_err_t load_sta_config(char* ssid, size_t ssid_len, char* password, size_t pass_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS: Failed to open '%s' R/O: %s", NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    err = nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS: Failed to read SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_get_str(nvs_handle, NVS_KEY_PASS, password, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS: Failed to read Password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    if (strlen(ssid) == 0) {
        ESP_LOGI(TAG, "NVS: Stored SSID is empty, treating as no credentials.");
        return ESP_FAIL; // No valid credentials if SSID is empty
    }
    ESP_LOGI(TAG, "NVS: Credentials loaded successfully for SSID: %s", ssid);
    return ESP_OK;
}

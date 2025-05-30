#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi_types.h" // For wifi_ap_record_t
#include <stddef.h> // For size_t

/**
 * @brief Default SSID for the Access Point mode when no credentials are found
 * or STA connection fails.
 */
#define WIFI_MANAGER_CONFIG_AP_SSID "ESP32_ECU_Config"

/**
 * @brief Maximum number of Access Points to list from a Wi-Fi scan.
 */
#define MAX_SCAN_RESULTS 20

/**
 * @brief Structure to hold simplified information about a scanned Access Point.
 */
typedef struct {
    char ssid[33];             /*!< SSID of the AP (32 chars + null terminator). */
    int8_t rssi;               /*!< Signal strength of the AP. */
    wifi_auth_mode_t authmode; /*!< Authentication mode of the AP. */
} wifi_ap_scan_result_t;

/**
 * @brief Initializes the Wi-Fi manager.
 *
 * This function orchestrates the Wi-Fi startup. It attempts to load saved STA credentials
 * from NVS and connect. If credentials are not found, or if the STA connection fails
 * after retries, it starts AP mode.
 * It also ensures necessary network interfaces and event loops are initialized if not already.
 * The HTTP server (for configuration in AP mode or data display in STA mode) is
 * typically started by the respective mode-starting functions called from here.
 */
void wifi_manager_init(void);

/**
 * @brief Saves Wi-Fi station (client) mode credentials to Non-Volatile Storage (NVS).
 *
 * @param ssid The SSID of the Wi-Fi network to save.
 * @param password The password for the Wi-Fi network.
 * @return esp_err_t ESP_OK on success, or an error code if NVS operation fails.
 */
esp_err_t wifi_manager_save_sta_config(const char *ssid, const char *password);

/**
 * @brief Stops AP mode and attempts to restart Wi-Fi in Station (STA) mode.
 *
 * This function is typically called after new STA credentials have been saved
 * (e.g., from the AP mode configuration web page). It stops the AP, de-initializes Wi-Fi,
 * and then calls `wifi_manager_init()` again, which will attempt to connect using the
 * newly saved credentials.
 */
void wifi_manager_start_sta_mode_from_ap(void);

/**
 * @brief Performs a Wi-Fi scan for available Access Points.
 *
 * This function triggers a Wi-Fi scan, retrieves the list of found APs,
 * and populates the provided results array.
 *
 * @param[out] results Pointer to an array of `wifi_ap_scan_result_t` structures
 *                     to store the scan results.
 * @param max_results The maximum number of results the `results` array can hold.
 * @return int The number of unique APs found and stored in `results`.
 *             Returns a negative value on error (e.g., -1).
 */
int wifi_manager_scan_ssids(wifi_ap_scan_result_t *results, uint16_t max_results);

/**
 * @brief Retrieves the current IP address of the ESP32 if connected in STA mode,
 *        or a status string indicating the current Wi-Fi state.
 *
 * @param[out] ip_str_buffer Buffer to store the IP address or status string.
 * @param buffer_len Size of the `ip_str_buffer`.
 * @return esp_err_t ESP_OK on success (buffer populated), or ESP_ERR_INVALID_ARG if buffer is invalid.
 */
esp_err_t wifi_manager_get_ip_info_str(char* ip_str_buffer, size_t buffer_len);

/**
 * @brief Checks if the ESP32 is currently connected to an AP in Station (STA) mode.
 *
 * @return true if connected as STA and has an IP address.
 * @return false otherwise (e.g., in AP mode, disconnected, or connecting).
 */
bool wifi_manager_is_sta_connected(void);

#endif // WIFI_MANAGER_H

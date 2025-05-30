#ifndef HTTP_SERVER_HANDLER_H
#define HTTP_SERVER_HANDLER_H

#include "esp_err.h"

/**
 * @brief Enum to define the operational mode of the HTTP server.
 */
typedef enum {
    HTTP_SERVER_MODE_AP,  /*!< Server runs in AP mode for Wi-Fi configuration. */
    HTTP_SERVER_MODE_STA  /*!< Server runs in STA mode for data display. */
} http_server_mode_t;

/**
 * @brief Starts the HTTP server.
 *
 * Initializes and starts the web server based on the specified mode.
 * In AP mode, it serves pages for Wi-Fi configuration and SSID scanning.
 * In STA mode, it serves pages for data display (e.g., RPM).
 * If the server is already running, it will be stopped and restarted with the new mode.
 *
 * @param mode The mode in which to start the server (AP or STA).
 * @return esp_err_t ESP_OK on success, or an error code if starting fails.
 */
esp_err_t http_server_start(http_server_mode_t mode);

/**
 * @brief Stops the HTTP server.
 *
 * If the server is running, this function stops it and frees associated resources.
 *
 * @return esp_err_t ESP_OK on success, or an error code if stopping fails (though typically returns ESP_OK).
 */
esp_err_t http_server_stop(void);

#endif // HTTP_SERVER_HANDLER_H

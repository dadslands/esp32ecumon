#include "http_server_handler.h"
#include "wifi_manager.h"
#include "can_reader.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <sys/param.h>
#include <stdio.h>

static const char *TAG = "http_server";
static httpd_handle_t server = NULL; // Stores the HTTP server instance handle

// --- HTML Content ---

/**
 * @brief HTML content for the Wi-Fi Configuration page served in AP mode.
 *
 * This page includes:
 * - A button to trigger a Wi-Fi scan.
 * - A dropdown list to display scanned SSIDs.
 * - Input fields for manually entering SSID and password.
 * - JavaScript to handle scanning, populating the dropdown, and updating the SSID field.
 */
static const char* HTML_WIFI_CONFIG_PAGE = R"rawliteral(
<!DOCTYPE HTML><html><head>
<title>ESP32 Wi-Fi Configuration</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f4f4f4; }
  .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); max-width: 500px; margin: auto; }
  h2 { color: #333; text-align: center; }
  label { display: block; margin-bottom: 8px; font-weight: bold; }
  input[type="text"], input[type="password"], select { width: calc(100% - 22px); padding: 10px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 4px; }
  button, input[type="submit"] { background-color: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 16px; margin-bottom:10px;}
  button:hover, input[type="submit"]:hover { background-color: #45a049; }
  button.scan { background-color: #007bff; }
  button.scan:hover { background-color: #0069d9; }
  .scan-status { font-size: 0.9em; color: #555; margin-bottom:10px; min-height:1.2em;}
  .msg { padding: 10px; margin-bottom: 20px; border-radius: 4px; }
  .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
  .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
</style>
</head><body>
<div class="container">
  <h2>Configure Wi-Fi</h2>
  <button class="scan" onclick="scanWifi()">Scan for Wi-Fi Networks</button>
  <div class="scan-status" id="scan-status"></div>
  <form method="POST" action="/connect">
    <label for="ssid-select">Select Network (or enter manually below):</label>
    <select id="ssid-select" name="ssid_select" onchange="updateSsidInput()">
      <option value="">-- Select a Network --</option>
    </select>
    <label for="ssid">SSID:</label>
    <input type="text" id="ssid" name="ssid" required><br>
    <label for="password">Password:</label>
    <input type="password" id="password" name="password"><br>
    <input type="submit" value="Connect">
  </form>
  <div class="msg" id="connect-message"></div>
</div>
<script>
  const ssidSelectElement = document.getElementById('ssid-select');
  const ssidInputElement = document.getElementById('ssid');
  const scanStatusElement = document.getElementById('scan-status');
  const connectMessageElement = document.getElementById('connect-message');

  function updateSsidInput() {
    if (ssidSelectElement.value) {
      ssidInputElement.value = ssidSelectElement.value;
    }
  }

  async function scanWifi() {
    scanStatusElement.textContent = 'Scanning...';
    ssidSelectElement.innerHTML = '<option value="">-- Scanning... --</option>';
    try {
      const response = await fetch('/scan_wifi');
      if (!response.ok) {
        throw new Error(`HTTP error! Status: ${response.status}`);
      }
      const networks = await response.json();

      ssidSelectElement.innerHTML = '<option value="">-- Select a Network --</option>';
      if (networks.length > 0) {
        networks.forEach(net => {
          const option = document.createElement('option');
          option.value = net.ssid;
          option.textContent = `${net.ssid} (RSSI: ${net.rssi}, Auth: ${authModeToString(net.auth)})`;
          ssidSelectElement.appendChild(option);
        });
        scanStatusElement.textContent = `Scan complete. Found ${networks.length} networks.`;
      } else {
        scanStatusElement.textContent = 'No networks found.';
      }
    } catch (error) {
      console.error('Error scanning Wi-Fi:', error);
      scanStatusElement.textContent = 'Error scanning Wi-Fi. Please try again.';
      ssidSelectElement.innerHTML = '<option value="">-- Scan Failed --</option>';
    }
  }

  function authModeToString(authMode) {
    switch(authMode) {
      case 0: return "OPEN"; case 1: return "WEP"; case 2: return "WPA_PSK";
      case 3: return "WPA2_PSK"; case 4: return "WPA_WPA2_PSK"; case 5: return "WPA2_ENTERPRISE";
      case 6: return "WPA3_PSK"; case 7: return "WPA2_WPA3_PSK"; default: return "UNKNOWN";
    }
  }
  const urlParams = new URLSearchParams(window.location.search);
  if (urlParams.has('msg')) { // Example: /?msg=Success&success=true
    connectMessageElement.textContent = decodeURIComponent(urlParams.get('msg'));
    connectMessageElement.className = urlParams.get('success') === 'true' ? 'msg success' : 'msg error';
  }
</script>
</body></html>
)rawliteral";

/**
 * @brief HTML content for the placeholder data display page served in STA mode.
 *
 * This page shows a placeholder for RPM and includes JavaScript to periodically
 * fetch and update the RPM value from the `/rpm_data` endpoint.
 */
static const char* HTML_STA_DATA_PAGE = R"rawliteral(
<!DOCTYPE HTML><html><head>
<title>ESP32 ECU Real-Time Data</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, Helvetica, sans-serif; margin: 0; padding: 0; background-color: #2c3e50; color: #ecf0f1; display: flex; justify-content: center; align-items: center; min-height: 100vh; text-align: center; }
  .container { background-color: #34495e; padding: 20px 40px; border-radius: 10px; box-shadow: 0 5px 15px rgba(0,0,0,0.2); }
  h2 { color: #1abc9c; border-bottom: 2px solid #1abc9c; padding-bottom: 10px; margin-bottom: 20px; }
  .data-item { margin-bottom: 25px; }
  .data-label { font-size: 1.2em; color: #bdc3c7; display: block; margin-bottom: 5px; }
  .data-value { font-size: 2.5em; font-weight: bold; color: #ffffff; background-color: #2c3e50; padding: 10px 15px; border-radius: 5px; min-width: 100px; display: inline-block; }
  #rpm-value { color: #f1c40f; }
  .error-msg { color: #e74c3c; margin-top: 15px; display: none; }
</style>
</head><body>
<div class="container">
  <h2>Vehicle Data</h2>
  <div class="data-item">
    <span class="data-label">Engine RPM</span>
    <span class="data-value" id="rpm-value">---</span>
  </div>
  <p class="error-msg" id="error-message"></p>
</div>
<script>
  const rpmElement = document.getElementById('rpm-value');
  const errorMessageElement = document.getElementById('error-message');
  async function fetchRpmData() {
    try {
      const response = await fetch('/rpm_data');
      if (!response.ok) { throw new Error(`HTTP error! Status: ${response.status}`); }
      const data = await response.json();
      rpmElement.textContent = data.rpm !== undefined ? data.rpm.toFixed(0) : 'N/A';
      errorMessageElement.style.display = 'none';
    } catch (error) {
      console.error('Error fetching RPM data:', error);
      rpmElement.textContent = 'Error'; // Indicate error on the RPM value itself
      errorMessageElement.textContent = 'Error fetching data. Retrying...';
      errorMessageElement.style.display = 'block';
    }
  }
  setInterval(fetchRpmData, 1500); // Fetch data every 1.5 seconds
  fetchRpmData(); // Initial fetch on page load
</script>
</body></html>
)rawliteral";


// --- URI Handlers ---

/**
 * @brief HTTP GET handler for serving the Wi-Fi configuration page in AP mode.
 */
static esp_err_t ap_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Serving AP mode Wi-Fi config page.");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, HTML_WIFI_CONFIG_PAGE, HTTPD_RESP_USE_STRLEN);
}

/**
 * @brief HTTP POST handler for receiving Wi-Fi credentials from the configuration page.
 *
 * Parses SSID and password from the POST data, saves them using `wifi_manager_save_sta_config`,
 * and then triggers a switch to STA mode using `wifi_manager_start_sta_mode_from_ap`.
 */
static esp_err_t connect_post_handler(httpd_req_t *req) {
    char buf[128]; // Buffer for POST data query string
    int ret, remaining = req->content_len;
    char ssid[33] = {0};
    char password[65] = {0};

    if (remaining >= sizeof(buf)) {
        ESP_LOGE(TAG, "POST content length (%d) too large for buffer (%d).", remaining, sizeof(buf));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }

    // Read the full request content
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req); // Request Timeout
        } else {
            ESP_LOGE(TAG, "Failed to receive POST data: %d", ret);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0'; // Null-terminate the received data
    ESP_LOGI(TAG, "Received POST data for /connect: %s", buf);

    // Parse SSID and Password from the POST data (e.g., "ssid=mySSID&password=myPassword")
    // Prioritize manually entered SSID, fallback to selected if manual is empty.
    // JS should ensure the manual ssid field is populated from select.
    if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) != ESP_OK || strlen(ssid) == 0) {
        if(httpd_query_key_value(buf, "ssid_select", ssid, sizeof(ssid)) != ESP_OK){
            ESP_LOGE(TAG, "Failed to parse SSID from POST data.");
             httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID not found in form data");
             return ESP_FAIL;
        }
    }
    httpd_query_key_value(buf, "password", password, sizeof(password)); // Okay if password is empty

    // Ensure null termination after parsing (httpd_query_key_value should handle this, but defensive)
    ssid[sizeof(ssid)-1] = '\0';
    password[sizeof(password)-1] = '\0';

    ESP_LOGI(TAG, "Parsed SSID: [%s]", ssid); // Password is not logged for security

    if (strlen(ssid) == 0) {
         ESP_LOGE(TAG, "SSID cannot be empty.");
         httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID cannot be empty");
         return ESP_FAIL;
    }

    // Save credentials via Wi-Fi manager
    esp_err_t save_err = wifi_manager_save_sta_config(ssid, password);
    if (save_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save Wi-Fi credentials: %s", esp_err_to_name(save_err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save credentials");
        return ESP_FAIL;
    }

    // Respond to client before restarting Wi-Fi (important!)
    const char* resp_msg = "Credentials received. Attempting to connect to Wi-Fi. This AP will turn off. If connection fails, AP will restart automatically.";
    httpd_resp_send(req, resp_msg, HTTPD_RESP_USE_STRLEN);

    // Trigger Wi-Fi restart to STA mode. Delay allows HTTP response to be sent.
    vTaskDelay(pdMS_TO_TICKS(1000));
    wifi_manager_start_sta_mode_from_ap();

    return ESP_OK;
}

/**
 * @brief HTTP GET handler for serving the main data display page in STA mode.
 */
static esp_err_t sta_get_root_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Serving STA mode data page.");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, HTML_STA_DATA_PAGE, HTTPD_RESP_USE_STRLEN);
}

/**
 * @brief HTTP GET handler for providing RPM data as JSON in STA mode.
 */
static esp_err_t rpm_data_get_handler(httpd_req_t *req) {
    float current_rpm = can_reader_get_latest_rpm();
    char json_buffer[64];

    // Format RPM into JSON string
    snprintf(json_buffer, sizeof(json_buffer), "{\"rpm\": %.0f}", current_rpm);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json_buffer, HTTPD_RESP_USE_STRLEN);
}

/**
 * @brief HTTP GET handler for providing scanned Wi-Fi networks as JSON in AP mode.
 * Uses cJSON library for robust JSON array creation.
 */
static esp_err_t scan_wifi_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Request received for /scan_wifi");
    wifi_ap_scan_result_t scan_results[MAX_SCAN_RESULTS];
    char *json_response_str = NULL;
    esp_err_t http_ret = ESP_OK;

    int count = wifi_manager_scan_ssids(scan_results, MAX_SCAN_RESULTS);

    if (count < 0) { // Error during scan
        ESP_LOGE(TAG, "Wi-Fi scan failed (error code %d).", count);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Wi-Fi scan failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Found %d networks from scan.", count);

    cJSON *root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        ESP_LOGE(TAG, "Failed to create cJSON root array for scan results.");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON array creation error");
        return ESP_FAIL;
    }

    for (int i = 0; i < count; i++) {
        cJSON *ap_json_obj = cJSON_CreateObject();
        if (ap_json_obj == NULL) {
            ESP_LOGE(TAG, "Failed to create cJSON object for AP info (iteration %d).", i);
            // Clean up already added items might be complex; fail all for simplicity
            cJSON_Delete(root_array);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON object creation error");
            return ESP_FAIL;
        }
        if (!cJSON_AddStringToObject(ap_json_obj, "ssid", scan_results[i].ssid) ||
            !cJSON_AddNumberToObject(ap_json_obj, "rssi", scan_results[i].rssi) ||
            !cJSON_AddNumberToObject(ap_json_obj, "auth", scan_results[i].authmode)) {
            ESP_LOGE(TAG, "Failed to add items to cJSON AP object (iteration %d).", i);
            cJSON_Delete(ap_json_obj);
            cJSON_Delete(root_array);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON item creation error");
            return ESP_FAIL;
        }
        cJSON_AddItemToArray(root_array, ap_json_obj);
    }

    json_response_str = cJSON_PrintUnformatted(root_array);
    if (json_response_str == NULL) {
        ESP_LOGE(TAG, "Failed to print cJSON array to string.");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON formatting error");
        http_ret = ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "Sending Wi-Fi scan results: %s", json_response_str);
        httpd_resp_set_type(req, "application/json");
        http_ret = httpd_resp_send(req, json_response_str, HTTPD_RESP_USE_STRLEN);
    }

    cJSON_Delete(root_array);
    if (json_response_str) free(json_response_str); // cJSON_PrintUnformatted allocates memory

    return http_ret;
}


// --- Server Start/Stop ---
/**
 * @brief Starts the HTTP server based on the system's Wi-Fi mode.
 * (Public function documentation is in http_server_handler.h)
 */
esp_err_t http_server_start(http_server_mode_t mode) {
    if (server != NULL) {
        ESP_LOGI(TAG, "HTTP server is already running. Stopping first to reconfigure for mode %d.", mode);
        http_server_stop();
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8; // Increased for /scan_wifi
    config.lru_purge_enable = true; // Enable LRU purge for inactive connections

    ESP_LOGI(TAG, "Starting HTTP server in %s mode.", (mode == HTTP_SERVER_MODE_AP) ? "AP" : "STA");

    if (httpd_start(&server, &config) == ESP_OK) {
        if (mode == HTTP_SERVER_MODE_AP) {
            httpd_uri_t ap_root_uri = { "/", HTTP_GET, ap_get_handler, NULL };
            httpd_register_uri_handler(server, &ap_root_uri);
            httpd_uri_t connect_post_uri = { "/connect", HTTP_POST, connect_post_handler, NULL };
            httpd_register_uri_handler(server, &connect_post_uri);
            httpd_uri_t scan_wifi_uri = { "/scan_wifi", HTTP_GET, scan_wifi_get_handler, NULL };
            httpd_register_uri_handler(server, &scan_wifi_uri);
            ESP_LOGI(TAG, "AP Mode URI handlers registered: /, /connect, /scan_wifi");
        } else { // HTTP_SERVER_MODE_STA
            httpd_uri_t sta_root_uri = { "/", HTTP_GET, sta_get_root_handler, NULL };
            httpd_register_uri_handler(server, &sta_root_uri);
            httpd_uri_t rpm_data_uri = { "/rpm_data", HTTP_GET, rpm_data_get_handler, NULL };
            httpd_register_uri_handler(server, &rpm_data_uri);
            ESP_LOGI(TAG, "STA Mode URI handlers registered: /, /rpm_data");
        }
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Error starting HTTP server!");
    server = NULL;
    return ESP_FAIL;
}

/**
 * @brief Stops the HTTP server.
 * (Public function documentation is in http_server_handler.h)
 */
esp_err_t http_server_stop(void) {
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping HTTP server.");
        // httpd_stop should unregister handlers and free resources.
        // Explicitly unregistering can be done but is not always necessary with httpd_stop.
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped.");
    } else {
        ESP_LOGI(TAG, "HTTP server not running, no action needed.");
    }
    return ESP_OK;
}

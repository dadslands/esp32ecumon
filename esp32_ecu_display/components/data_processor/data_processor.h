#ifndef DATA_PROCESSOR_H
#define DATA_PROCESSOR_H

#include "esp_err.h"

// Example: Define a structure for raw data coming from can_reader
// typedef struct {
//     float raw_rpm;
//     int raw_speed;
//     // ... other raw values
// } raw_vehicle_data_t;

// Example: Define a structure for processed data to be used by display_manager or others
// typedef struct {
//     float processed_rpm_avg;
//     int current_speed_kmh;
//     float coolant_temp_c;
//     // ... other processed values or flags
//     bool high_coolant_temp_alert;
// } processed_vehicle_data_t;


// Initialize the Data Processor module
esp_err_t data_processor_init(void);

// Function to input raw data for processing
// esp_err_t data_processor_input_raw_data(const raw_vehicle_data_t *raw_data);

// Function to get the latest processed data
// esp_err_t data_processor_get_processed_data(processed_vehicle_data_t *out_data);

// Placeholder for the data processing task function (if it runs as a separate task)
// void data_processor_task(void *pvParameters);

#endif // DATA_PROCESSOR_H

#include "config.h"
#include "event.h"
#include "network/ethernet.h"

#include <freertos/FreeRTOS.h>

static void initializeLogger()
{
    esp_log_level_set(MAIN_LOGGER_TAG, MAIN_LOGGER_LEVEL);
    esp_log_level_set(EVENT_LOGGER_TAG, EVENT_LOGGER_LEVEL);
    esp_log_level_set(NETWORK_LOGGER_TAG, NETWORK_LOGGER_LEVEL);
}

void app_main()
{
    initializeLogger();
    vTaskDelay(STARTUP_DELAY_MS / portTICK_PERIOD_MS);

    ESP_LOGI(MAIN_LOGGER_TAG, "Probe initialization");
    initializeEvent();
    initializeEthernet();

    ESP_LOGI(MAIN_LOGGER_TAG, "Main loop");
    while(1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

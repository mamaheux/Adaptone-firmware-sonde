#include "config.h"
#include "event.h"
#include "network/ethernet.h"
#include "network/sntp.h"
#include "network/discovery.h"

#include <freertos/FreeRTOS.h>

#include <time.h>

#define STRFTIME_BUFFER_SIZE 64
#define UTC_LOG_INTERVAL_MS 10000

static void initializeLogger()
{
    esp_log_level_set(MAIN_LOGGER_TAG, MAIN_LOGGER_LEVEL);
    esp_log_level_set(EVENT_LOGGER_TAG, EVENT_LOGGER_LEVEL);
    esp_log_level_set(NETWORK_LOGGER_TAG, NETWORK_LOGGER_LEVEL);
}

static void logCurrentUtc()
{
    char buffer[STRFTIME_BUFFER_SIZE];
    struct timeval tv;
    struct tm timeinfo;

    if (gettimeofday(&tv, NULL) == 0)
    {
        localtime_r(&tv.tv_sec, &timeinfo);
        strftime(buffer, sizeof(buffer), "%c", &timeinfo);
        ESP_LOGI(MAIN_LOGGER_TAG, "The current UTC time is: %s (us = %ld)", buffer, tv.tv_usec);
    }
}

void app_main()
{
    initializeLogger();
    vTaskDelay(STARTUP_DELAY_MS / portTICK_PERIOD_MS);

    ESP_LOGI(MAIN_LOGGER_TAG, "Initialization");
    initializeEvent();
    initializeEthernet();
    initializeStnp();
    initializeDiscovery();

    ESP_LOGI(MAIN_LOGGER_TAG, "Task start");
    startDiscovery();
    while(1)
    {
        logCurrentUtc();
        vTaskDelay(UTC_LOG_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

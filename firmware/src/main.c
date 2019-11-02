#include "config.h"
#include "event.h"
#include "network/ethernet.h"
#include "network/sntp.h"
#include "network/discovery.h"
#include "network/communication.h"
#include "network/communication.h"
#include "sound.h"

#include <time.h>

#define STRFTIME_BUFFER_SIZE 64

static void initializeLogger()
{
    esp_log_level_set(MAIN_LOGGER_TAG, MAIN_LOGGER_LEVEL);
    esp_log_level_set(EVENT_LOGGER_TAG, EVENT_LOGGER_LEVEL);
    esp_log_level_set(NETWORK_LOGGER_TAG, NETWORK_LOGGER_LEVEL);
    esp_log_level_set(SOUND_LOGGER_TAG, SOUND_LOGGER_LEVEL);
}

static void messageHandler(uint8_t* buffer, size_t size)
{
    ESP_LOGI(MAIN_LOGGER_TAG, "Message received");
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
    vTaskDelay(CONFIG_STARTUP_DELAY_MS / portTICK_PERIOD_MS);

    ESP_LOGI(MAIN_LOGGER_TAG, "Initialization");
    initializeEvent();
    initializeEthernet();
    initializeStnp();
    initializeDiscovery();
    initializeCommunication(messageHandler);
    initializeSound();

    ESP_LOGI(MAIN_LOGGER_TAG, "Task start");
    startDiscovery();
    startCommunication();
    startSound();

    while(1)
    {
        logCurrentUtc();
        vTaskDelay(CONFIG_UTC_LOG_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

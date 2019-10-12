#ifndef CONFIG_H
#define CONFIG_H

#include <arpa/inet.h>
#include <esp_log.h>

#define MAIN_LOGGER_TAG "Main"
#define MAIN_LOGGER_LEVEL ESP_LOG_DEBUG

#define EVENT_LOGGER_TAG "Event"
#define EVENT_LOGGER_LEVEL ESP_LOG_DEBUG

#define NETWORK_LOGGER_TAG "Network"
#define NETWORK_LOGGER_LEVEL ESP_LOG_DEBUG

#define STARTUP_DELAY_MS 1000

#endif

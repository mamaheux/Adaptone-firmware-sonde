#include "event.h"
#include "config.h"
#include "network/ethernet.h"

#include <esp_event_loop.h>
#include <esp_event.h>
#include <esp_err.h>

#include <string.h>

static esp_err_t eventHandler(void *ctx, system_event_t *event)
{
    uint8_t macAddress[MAC_ADDRESS_SIZE] = { 0 };
    tcpip_adapter_ip_info_t ip;

    switch (event->event_id)
    {
        case SYSTEM_EVENT_ETH_CONNECTED:
            esp_eth_get_mac(macAddress);
            ESP_LOGI(EVENT_LOGGER_TAG, "Ethernet link up");
            ESP_LOGI(EVENT_LOGGER_TAG, "Ethernet HW address %02x:%02x:%02x:%02x:%02x:%02x",
                macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            ESP_LOGI(EVENT_LOGGER_TAG, "Ethernet link down");
            break;
        case SYSTEM_EVENT_ETH_START:
            ESP_LOGI(EVENT_LOGGER_TAG, "Ethernet started");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));
            ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(ESP_IF_ETH, &ip));
            ESP_LOGI(EVENT_LOGGER_TAG, "Ethernet got IP address");
            ESP_LOGI(EVENT_LOGGER_TAG, "ETHIP:" IPSTR, IP2STR(&ip.ip));
            ESP_LOGI(EVENT_LOGGER_TAG, "ETHMASK:" IPSTR, IP2STR(&ip.netmask));
            ESP_LOGI(EVENT_LOGGER_TAG, "ETHGW:" IPSTR, IP2STR(&ip.gw));
            break;
        case SYSTEM_EVENT_ETH_STOP:
            ESP_LOGI(EVENT_LOGGER_TAG, "Ethernet stopped");
            break;
        default:
            ESP_LOGW(EVENT_LOGGER_TAG, "Not handled event");
            break;
    }

    return ESP_OK;
}

void initializeEvent()
{
    ESP_LOGI(EVENT_LOGGER_TAG, "Initialization");
    ESP_ERROR_CHECK(esp_event_loop_init(eventHandler, NULL));
}

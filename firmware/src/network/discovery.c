#include "network/discovery.h"
#include "network/utils.h"
#include "config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

#define DISCOVERY_RESQUEST_SIZE 4
#define DISCOVERY_RESQUEST_ID 0

#define DISCOVERY_RESPONSE_SIZE 4

static struct sockaddr_in bindAddress;
static uint8_t receivingBuffer[CONFIG_DISCOVERY_RECEIVING_BUFFER_SIZE];

static int createSocket()
{
    int broadcast = 1;
    int socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socketHandle < 0) 
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }

    if (setsockopt(socketHandle, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to enable broadcast: errno %d", errno);
        freeSocket(socketHandle);
        return -1;
    }

    if (bind(socketHandle, (struct sockaddr*)&bindAddress, sizeof(bindAddress)) < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to bind: errno %d", errno);
        freeSocket(socketHandle);
        return -1;
    }

    return socketHandle;
}

static int isDiscoveryRequest(uint8_t* buffer, int size)
{
    return size == DISCOVERY_RESQUEST_SIZE && 
        ntohl(*(uint32_t*)buffer) == DISCOVERY_RESQUEST_ID;
 }

static int sendDiscoveryResponse(int socketHandle, struct sockaddr_in* sourceAddress)
{
    int flags = 0;
    uint8_t buffer[DISCOVERY_RESPONSE_SIZE] = { 0, 0, 0, 1 };

    int error = sendto(socketHandle,
        buffer,
        DISCOVERY_RESPONSE_SIZE,
        flags,
        (struct sockaddr*)sourceAddress,
        sizeof(*sourceAddress));

    if (error < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "sendto failed: errno %d", errno);
        return 0;
    }
    return 1;
}

static int handleDiscoveryMessage(int socketHandle)
{
    int flags = 0;
    struct sockaddr_in sourceAddress;
    socklen_t socklen = sizeof(sourceAddress);

    int size = recvfrom(socketHandle,
        receivingBuffer,
        CONFIG_DISCOVERY_RECEIVING_BUFFER_SIZE,
        flags,
        (struct sockaddr*)&sourceAddress,
        &socklen);

    if (size < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "recvfrom failed: errno %d", errno);
        return 0;
    }

    if (isDiscoveryRequest(receivingBuffer, size))
    {
        ESP_LOGI(NETWORK_LOGGER_TAG, "Discovery request received");
        return sendDiscoveryResponse(socketHandle, &sourceAddress);
    }

    return 1;
}

static void discoveryTask(void* parameters)
{
    while (1)
    {
        int socketHandle = createSocket();

        while (socketHandle > 0 && handleDiscoveryMessage(socketHandle));

        if (socketHandle > 0)
        {
            ESP_LOGE(NETWORK_LOGGER_TAG, "Shutting down socket and restarting...");            
            freeSocket(socketHandle);
        }

        vTaskDelay(CONFIG_DISCOVERY_SOCKET_CREATION_INTERVAL_MS / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void initializeDiscovery()
{
    ESP_LOGI(NETWORK_LOGGER_TAG, "Discovery initialization");
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(CONFIG_DISCOVERY_PORT);
}

void startDiscovery()
{
    xTaskCreate(discoveryTask, 
        "discovery",
        CONFIG_DISCOVERY_TASK_STACK_SIZE,
        NULL,
        CONFIG_DISCOVERY_TASK_PRIORITY,
        NULL);
}

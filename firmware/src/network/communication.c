#include "network/communication.h"
#include "network/utils.h"
#include "config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

#include <string.h>

#define INITIALIZATION_RESQUEST_SIZE 16
#define INITIALIZATION_RESQUEST_ID 2
#define INITIALIZATION_RESQUEST_SAMPLE_FREQUENCY_OFFSET 8
#define INITIALIZATION_RESQUEST_SAMPLE_FORMAT_OFFSET 12

#define INITIALIZATION_RESPONSE_SIZE 14
#define INITIALIZATION_RESPONSE_PROBE_ID_OFFSET 10

#define HEARTBEAT_SIZE 4
#define HEARTBEAT_ID 4


static MessageHandler messageHandler;
static struct sockaddr_in tcpListenerAddress;

static struct sockaddr_in clientAddress;
int tcpClientSocketHandle;
int udpClientSocketHandle;
SemaphoreHandle_t clientMutex;

static uint8_t receivingBuffer[CONFIG_COMMUNICATION_RECEIVING_BUFFER_SIZE];

static int setReceivingTimeout(int socketHandle)
{
    struct timeval tv;
    tv.tv_sec = CONFIG_COMMUNICATION_TIMEOUT_MS / 1000;
    tv.tv_usec = (CONFIG_COMMUNICATION_TIMEOUT_MS % 1000) * 1000;

    return setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

static int createTcpListenerSocket()
{
    int reuseaddr = 1;
    int socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socketHandle < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }

    if (setsockopt(socketHandle, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to enable REUSEADDR: errno %d", errno);
        freeSocket(socketHandle);
        return -1;
    }

    if (setReceivingTimeout(socketHandle) < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to set SO_RCVTIMEO: errno %d", errno);
        freeSocket(socketHandle);
        return -1;
    }

    if (bind(socketHandle, (struct sockaddr*)&tcpListenerAddress, sizeof(tcpListenerAddress)) < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to bind: errno %d", errno);
        freeSocket(socketHandle);
        return -1;
    }

    if (listen(socketHandle, CONFIG_COMMUNICATION_TCP_LISTENER_QUEUE_SIZE) != 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to listen: errno %d", errno);
        freeSocket(socketHandle);
        return -1;
    }

    return socketHandle;
}

static int createUdpClientSocket()
{
    int socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socketHandle < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }
    
    return socketHandle;
}

static int hasPayload(uint32_t messageId)
{
    switch (messageId)
    {
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
            return 1;

        default:
            return 0;
    }
}

static int receiveMessage(int socketHandle, uint8_t* buffer, size_t bufferSize)
{
    int flags = 0;
    size_t receivedDataSize = 0;
    uint32_t messageId;    
    if (recv(socketHandle, buffer + receivedDataSize, sizeof(messageId), flags) != sizeof(messageId))
    {
        return -1;
    }
    messageId = ntohl(*(uint32_t*)(buffer + receivedDataSize));
    receivedDataSize += sizeof(messageId);

    if (!hasPayload(messageId))
    {
        return receivedDataSize;
    }

    uint32_t payloadSize;
    if (recv(socketHandle, buffer + receivedDataSize, sizeof(payloadSize), flags) != sizeof(payloadSize))
    {
        return -1;
    }
    payloadSize = ntohl(*(uint32_t*)(buffer + receivedDataSize));
    receivedDataSize += sizeof(payloadSize);

    uint32_t receivedPayloadSize = 0;
    while (receivedPayloadSize < payloadSize && receivedDataSize < bufferSize)
    {
        int size = recv(socketHandle, buffer + receivedDataSize, payloadSize - receivedPayloadSize, flags);
        if (size < 0)
        {
            return -1;
        }

        receivedPayloadSize += size;
        receivedDataSize += size;
    }

    return receivedDataSize;
}

static int isInitializationRequest(uint8_t* buffer, int size)
{
    return size == INITIALIZATION_RESQUEST_SIZE && 
        ntohl(*(uint32_t*)buffer) == INITIALIZATION_RESQUEST_ID;
}

static int isInitializationCompatible(uint8_t* initializationRequest)
{
    return ntohl(*(uint32_t*)(initializationRequest + INITIALIZATION_RESQUEST_SAMPLE_FREQUENCY_OFFSET)) == CONFIG_SOUND_SAMPLE_FREQUENCY &&
        ntohl(*(uint32_t*)(initializationRequest + INITIALIZATION_RESQUEST_SAMPLE_FORMAT_OFFSET)) == CONFIG_SOUND_SAMPLE_FORMAT;
}

static void sendInitializationResponse(int socketHandle, int isCompatible)
{
    int flags = 0;
    uint8_t buffer[INITIALIZATION_RESPONSE_SIZE] =
    { 
        0, 0, 0, 3,
        0, 0, 0, 6,
        (uint8_t)isCompatible, (uint8_t)CONFIG_PROBE_IS_MASTER,
        0, 0, 0, 0
    };
    *(uint32_t*)(buffer + INITIALIZATION_RESPONSE_PROBE_ID_OFFSET) = htonl(CONFIG_PROBE_ID);

    send(socketHandle, buffer, INITIALIZATION_RESPONSE_SIZE, flags);
}

static int acceptConnection(int tcpListenerSocketHandle)
{
    struct sockaddr_in sourceAddress;
    uint addressSize = sizeof(sourceAddress);
    int tcpSocketHandle = -1;
    
    do
    {
        tcpSocketHandle = accept(tcpListenerSocketHandle, (struct sockaddr*)&sourceAddress, &addressSize);
        if (tcpSocketHandle < 0 && errno != EAGAIN)
        {
            ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to accept: errno %d", errno);
            return 0;
        }
    } while (tcpSocketHandle < 0);

    if (setReceivingTimeout(tcpSocketHandle) < 0)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to set SO_RCVTIMEO: errno %d", errno);
        freeSocket(tcpSocketHandle);
        return 0;
    }

    int size = receiveMessage(tcpSocketHandle, receivingBuffer, CONFIG_COMMUNICATION_RECEIVING_BUFFER_SIZE);
    if (size < 0 && !isInitializationRequest(receivingBuffer, size))
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to receive the initialization request: errno %d", errno);
        freeSocket(tcpSocketHandle);
        return 0;
    }

    int isCompatible = isInitializationCompatible(receivingBuffer);
    sendInitializationResponse(tcpSocketHandle, isCompatible);

    if (!isCompatible)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Not compatible initialization");
        freeSocket(tcpSocketHandle);
        return 0;
    }
    
    xSemaphoreTake(clientMutex, portMAX_DELAY);
    
    int udpSocketHandle = createUdpClientSocket();
    if (udpSocketHandle < 0)
    {
        freeSocket(tcpSocketHandle);
        return 0;
    }

    udpClientSocketHandle = udpSocketHandle;
    memcpy(&clientAddress, &sourceAddress, sizeof(sourceAddress));
    tcpClientSocketHandle = tcpSocketHandle;

    xSemaphoreGive(clientMutex);

    ESP_LOGI(NETWORK_LOGGER_TAG, "Inbound connection accepted");
    return 1;
}

static int isHeartbeatMessage(uint8_t* buffer, int size)
{
    return size == HEARTBEAT_SIZE && 
        ntohl(*(uint32_t*)buffer) == HEARTBEAT_ID;
}

static void sendHeartbeatMessage(int socketHandle)
{
    int flags = 0;
    uint8_t buffer[HEARTBEAT_SIZE] =
    { 
        0, 0, 0, 0
    };
    *(uint32_t*)(buffer) = htonl(HEARTBEAT_ID);

    send(socketHandle, buffer, HEARTBEAT_SIZE, flags);
}

static void handleMessages()
{
    uint32_t lastHeatbeatTimestamp = esp_log_timestamp();
    while (1)
    {
        int size = receiveMessage(tcpClientSocketHandle, receivingBuffer, CONFIG_COMMUNICATION_RECEIVING_BUFFER_SIZE);
        if (size < 0 && errno != EAGAIN)
        {
            ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to receive a request: errno %d", errno);
            return;
        }

        if (isHeartbeatMessage(receivingBuffer, size))
        {
            ESP_LOGI(NETWORK_LOGGER_TAG, "Heartbeat received");
            sendHeartbeatMessage(tcpClientSocketHandle);
            lastHeatbeatTimestamp = esp_log_timestamp();
        }
        else if (size > 0)
        {
            messageHandler(receivingBuffer, size);
        }

        if ((esp_log_timestamp() - lastHeatbeatTimestamp) > CONFIG_COMMUNICATION_HEARTBEAT_TIMEOUT_MS)
        {
            ESP_LOGE(NETWORK_LOGGER_TAG, "Heartbeat timeout");
            return;
        }
    }
}

static void communicationTask(void* parameters)
{
    while (1)
    {
        int tcpListenerSocketHandle = createTcpListenerSocket();

        if (tcpListenerSocketHandle > 0)
        {
            if (acceptConnection(tcpListenerSocketHandle))
            {
                handleMessages();
            }
        }

        xSemaphoreTake(clientMutex, portMAX_DELAY);
        if (tcpClientSocketHandle > 0)
        {
            freeSocket(tcpClientSocketHandle);
            freeSocket(udpClientSocketHandle);
            tcpClientSocketHandle = -1;
            udpClientSocketHandle = -1;
        }
        xSemaphoreGive(clientMutex);

        if (tcpListenerSocketHandle > 0)
        {
            ESP_LOGE(NETWORK_LOGGER_TAG, "Shutting down socket and restarting...");            
            freeSocket(tcpListenerSocketHandle);
        }

        vTaskDelay(CONFIG_COMMUNICATION_SOCKET_CREATION_INTERVAL_MS / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void initializeCommunication(MessageHandler userMessageHandler)
{
    ESP_LOGI(NETWORK_LOGGER_TAG, "Communication initialization");
    messageHandler = userMessageHandler;

    tcpListenerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    tcpListenerAddress.sin_family = AF_INET;
    tcpListenerAddress.sin_port = htons(CONFIG_COMMUNICATION_TCP_PORT);

    tcpClientSocketHandle = -1;
    udpClientSocketHandle = -1;
    clientMutex = xSemaphoreCreateMutex();
    if (clientMutex == NULL)
    {
        ESP_LOGE(NETWORK_LOGGER_TAG, "Unable to create the client mutex");
    }
}

void startCommunication()
{
    xTaskCreate(communicationTask, 
        "communication",
        CONFIG_COMMUNICATION_TASK_STACK_SIZE,
        NULL,
        CONFIG_COMMUNICATION_TASK_PRIORITY,
        NULL);
}

void sendTcp(uint8_t* buffer, size_t size)
{
    int flags = 0;
    xSemaphoreTake(clientMutex, portMAX_DELAY);
    if (tcpClientSocketHandle >= 0)
    {
        send(tcpClientSocketHandle, buffer, size, flags);
    }
    xSemaphoreGive(clientMutex);
}

void sendUdp(uint8_t* buffer, size_t size)
{
    int flags = 0;
    xSemaphoreTake(clientMutex, portMAX_DELAY);
    if (udpClientSocketHandle >= 0)
    {
        sendto(udpClientSocketHandle,
            buffer,
            size,
            flags,
            (struct sockaddr*)&clientAddress,
            sizeof(clientAddress));
    }
    xSemaphoreGive(clientMutex);
}

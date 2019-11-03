#ifndef NETWORK_COMMUNICATION_H
#define NETWORK_COMMUNICATION_H

#include <stdint.h>
#include <stddef.h>

typedef void (*RecordMessageHandler)(uint8_t recordHour,
    uint8_t recordMinute,
    uint8_t recordSecond,
    uint16_t recordMs,
    uint16_t durationMs,
    uint8_t recordId);

void initializeCommunication(RecordMessageHandler recordMessageHandler);
void startCommunication();

void sendTcp(uint8_t* buffer, size_t size);
void sendUdp(uint8_t* buffer, size_t size);

#endif

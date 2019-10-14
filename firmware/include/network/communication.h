#ifndef NETWORK_COMMUNICATION_H
#define NETWORK_COMMUNICATION_H

#include <stdint.h>
#include <stddef.h>

typedef void (*MessageHandler)(uint8_t* buffer, size_t size);

void initializeCommunication(MessageHandler messageHandler);
void startCommunication();

void sendTcp(uint8_t* buffer, size_t size);
void sendUdp(uint8_t* buffer, size_t size);

#endif

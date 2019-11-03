#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

void initializeSound();
void startSound();

void recordSound(uint8_t recordHour,
    uint8_t recordMinute,
    uint8_t recordSecond,
    uint16_t recordMs,
    uint16_t durationMs,
    uint8_t recordId);

#endif

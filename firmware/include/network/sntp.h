#ifndef NETWORK_SNTP_H
#define NETWORK_SNTP_H

#include <lwip/apps/sntp.h>

#define CONFIG_SNTP_OPERATING_MODE SNTP_OPMODE_POLL
#define CONFIG_SNTP_SERVER_NAME "pool.ntp.org"
#define CONFIG_SNTP_TIME_ZONE "GMT" //UTC

void initializeStnp();

#endif
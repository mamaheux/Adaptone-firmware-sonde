#include "network/sntp.h"
#include "config.h"

void initializeStnp()
{
    ESP_LOGI(NETWORK_LOGGER_TAG, "SNTP initialization");
    sntp_setoperatingmode(CONFIG_SNTP_OPERATING_MODE);
    sntp_setservername(0, CONFIG_SNTP_SERVER_NAME);

    setenv("TZ", CONFIG_SNTP_TIME_ZONE, 1);
    tzset();

    sntp_init();
}

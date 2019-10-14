#include "network/ethernet.h"
#include "config.h"

#include <esp_err.h>
#include <tcpip_adapter.h>

static void gpioConfig(void)
{
    phy_rmii_configure_data_interface_pins();
    phy_rmii_smi_configure_pins(CONFIG_ETHERNET_PIN_SMI_MDC, CONFIG_ETHERNET_PIN_SMI_MDIO);
}

void initializeEthernet()
{
    ESP_LOGI(NETWORK_LOGGER_TAG, "Ethernet initialization");
    tcpip_adapter_init();

    eth_config_t config = CONFIG_ETHERNET_PHY_CONFIG;
    config.phy_addr = CONFIG_ETHERNET_PHY_ADDRESS;
    config.gpio_config = gpioConfig;
    config.tcpip_input = tcpip_adapter_eth_input;
    config.clock_mode = CONFIG_ETHERNET_PHY_CLOCK_MODE;

    ESP_ERROR_CHECK(esp_eth_init(&config));
    ESP_ERROR_CHECK(esp_eth_enable());
}

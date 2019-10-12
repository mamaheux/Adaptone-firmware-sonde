#ifndef NETWORK_ETHERNET_H
#define NETWORK_ETHERNET_H

#include <esp_eth.h>
#include <eth_phy/phy_lan8720.h>

#define MAC_ADDRESS_SIZE 6

#define CONFIG_ETHERNET_PHY_CONFIG phy_lan8720_default_ethernet_config
#define CONFIG_PHY_ADDRESS PHY0
#define CONFIG_PHY_CLOCK_MODE ETH_CLOCK_GPIO0_IN
#define CONFIG_PIN_SMI_MDC 16
#define CONFIG_PIN_SMI_MDIO 17

void initializeEthernet();

#endif

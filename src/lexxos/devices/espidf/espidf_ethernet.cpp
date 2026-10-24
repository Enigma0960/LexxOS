#include <lexxos/devices/espidf/espidf_ethernet.h>

#include <cstdio>
#include <cstring>

#include <esp_err.h>
#include <esp_eth.h>
#include <esp_eth_mac_esp.h>
#include <esp_eth_phy.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <soc/soc_caps.h>

namespace lexxos::devices::espidf_ethernet {
namespace {

	struct Runtime {
		esp_eth_handle_t handle{nullptr};
		esp_eth_netif_glue_handle_t glue{nullptr};
		esp_netif_t* netif{nullptr};
		Status status{};
		bool eventHandlersRegistered{false};
	};

	Runtime g_runtime{};

	void setError(esp_err_t result) {
		g_runtime.status.lastError = result;
		auto const* name = esp_err_to_name(result);
		std::snprintf(g_runtime.status.lastErrorName, sizeof(g_runtime.status.lastErrorName), "%s", name != nullptr ? name : "ESP_ERR_UNKNOWN");
	}

	void clearError() {
		setError(ESP_OK);
	}

	void clearIp() {
		g_runtime.status.gotIp = false;
		g_runtime.status.ip[0] = '\0';
		g_runtime.status.netmask[0] = '\0';
		g_runtime.status.gateway[0] = '\0';
	}

	void refreshMac() {
		if (g_runtime.handle == nullptr) {
			g_runtime.status.macValid = false;
			return;
		}
		g_runtime.status.macValid = esp_eth_ioctl(g_runtime.handle, ETH_CMD_G_MAC_ADDR, g_runtime.status.mac) == ESP_OK;
	}

	char uppercaseHexDigit(std::uint8_t value) {
		return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
	}

	void refreshHostnameFromMac() {
		if (!g_runtime.status.macValid) {
			g_runtime.status.hostname[0] = '\0';
			return;
		}
		auto const byte4 = g_runtime.status.mac[4];
		auto const byte5 = g_runtime.status.mac[5];
		std::snprintf(g_runtime.status.hostname,
		    sizeof(g_runtime.status.hostname),
		    "lexxos-%c%c%c%c",
		    uppercaseHexDigit(static_cast<std::uint8_t>((byte4 >> 4U) & 0x0FU)),
		    uppercaseHexDigit(static_cast<std::uint8_t>(byte4 & 0x0FU)),
		    uppercaseHexDigit(static_cast<std::uint8_t>((byte5 >> 4U) & 0x0FU)),
		    uppercaseHexDigit(static_cast<std::uint8_t>(byte5 & 0x0FU)));
	}

	void refreshLinkDetails() {
		if (g_runtime.handle == nullptr) {
			g_runtime.status.speedValid = false;
			g_runtime.status.duplexValid = false;
			return;
		}

		eth_speed_t speed{};
		if (esp_eth_ioctl(g_runtime.handle, ETH_CMD_G_SPEED, &speed) == ESP_OK) {
			g_runtime.status.speedValid = true;
			g_runtime.status.speedMbps = speed == ETH_SPEED_100M ? 100U : 10U;
		} else {
			g_runtime.status.speedValid = false;
		}

		eth_duplex_t duplex{};
		if (esp_eth_ioctl(g_runtime.handle, ETH_CMD_G_DUPLEX_MODE, &duplex) == ESP_OK) {
			g_runtime.status.duplexValid = true;
			g_runtime.status.fullDuplex = duplex == ETH_DUPLEX_FULL;
		} else {
			g_runtime.status.duplexValid = false;
		}
	}

	bool ensureNetworkCore() {
		auto result = esp_netif_init();
		if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
			setError(result);
			return false;
		}

		result = esp_event_loop_create_default();
		if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
			setError(result);
			return false;
		}
		return true;
	}

	void destroyPartialDriver(esp_eth_mac_t* mac, esp_eth_phy_t* phy) {
		if (mac != nullptr) {
			mac->del(mac);
		}
		if (phy != nullptr) {
			phy->del(phy);
		}
	}

	void ethEventHandler(void*, esp_event_base_t, int32_t eventId, void* eventData) {
		if (eventData != nullptr) {
			g_runtime.handle = *static_cast<esp_eth_handle_t*>(eventData);
		}

		switch (eventId) {
			case ETHERNET_EVENT_START:
				g_runtime.status.started = true;
				break;
			case ETHERNET_EVENT_STOP:
				g_runtime.status.started = false;
				g_runtime.status.linkUp = false;
				g_runtime.status.speedValid = false;
				g_runtime.status.duplexValid = false;
				clearIp();
				break;
			case ETHERNET_EVENT_CONNECTED:
				g_runtime.status.linkUp = true;
				refreshMac();
				refreshLinkDetails();
				break;
			case ETHERNET_EVENT_DISCONNECTED:
				g_runtime.status.linkUp = false;
				g_runtime.status.speedValid = false;
				g_runtime.status.duplexValid = false;
				clearIp();
				break;
			default:
				break;
		}
	}

	void gotIpEventHandler(void*, esp_event_base_t, int32_t, void* eventData) {
		auto* event = static_cast<ip_event_got_ip_t*>(eventData);
		if (event == nullptr) {
			return;
		}
		g_runtime.status.gotIp = true;
		esp_ip4addr_ntoa(&event->ip_info.ip, g_runtime.status.ip, sizeof(g_runtime.status.ip));
		esp_ip4addr_ntoa(&event->ip_info.netmask, g_runtime.status.netmask, sizeof(g_runtime.status.netmask));
		esp_ip4addr_ntoa(&event->ip_info.gw, g_runtime.status.gateway, sizeof(g_runtime.status.gateway));
	}

	bool installDriver(lexxos::platform::BoardDescriptor const& board) {
#if !SOC_EMAC_SUPPORTED
		(void) board;
		g_runtime.status = {};
		setError(ESP_ERR_NOT_SUPPORTED);
		return false;
#else
		auto const& pins = board.ethernet;
		if (!pins.isValid()) {
			g_runtime.status = {};
			g_runtime.status.supported = false;
			setError(ESP_ERR_NOT_SUPPORTED);
			return false;
		}

		g_runtime.status.supported = true;
		if (g_runtime.status.initialized) {
			return true;
		}
		if (!ensureNetworkCore()) {
			return false;
		}

		eth_mac_config_t macConfig = ETH_MAC_DEFAULT_CONFIG();
		eth_phy_config_t phyConfig = ETH_PHY_DEFAULT_CONFIG();
		phyConfig.phy_addr = pins.phyAddress;
		phyConfig.reset_gpio_num = pins.reset.isValid() ? pins.reset.number : -1;

		eth_esp32_emac_config_t emacConfig{};
		emacConfig.smi_gpio.mdc_num = pins.mdc.number;
		emacConfig.smi_gpio.mdio_num = pins.mdio.number;
		emacConfig.interface = EMAC_DATA_INTERFACE_RMII;
		emacConfig.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
		emacConfig.clock_config.rmii.clock_gpio = pins.clock.number;
		emacConfig.dma_burst_len = ETH_DMA_BURST_LEN_32;
		emacConfig.intr_priority = 0;
		emacConfig.mdc_freq_hz = 0;
	#if SOC_EMAC_USE_MULTI_IO_MUX
		emacConfig.emac_dataif_gpio.rmii.tx_en_num = pins.txEnable.number;
		emacConfig.emac_dataif_gpio.rmii.txd0_num = pins.txd0.number;
		emacConfig.emac_dataif_gpio.rmii.txd1_num = pins.txd1.number;
		emacConfig.emac_dataif_gpio.rmii.crs_dv_num = pins.crsDv.number;
		emacConfig.emac_dataif_gpio.rmii.rxd0_num = pins.rxd0.number;
		emacConfig.emac_dataif_gpio.rmii.rxd1_num = pins.rxd1.number;
	#endif
	#if !SOC_EMAC_RMII_CLK_OUT_INTERNAL_LOOPBACK
		emacConfig.clock_config_out_in.rmii.clock_mode = EMAC_CLK_EXT_IN;
		emacConfig.clock_config_out_in.rmii.clock_gpio = -1;
	#endif

		auto* mac = esp_eth_mac_new_esp32(&emacConfig, &macConfig);
		if (mac == nullptr) {
			setError(ESP_FAIL);
			return false;
		}
		auto* phy = esp_eth_phy_new_generic(&phyConfig);
		if (phy == nullptr) {
			destroyPartialDriver(mac, nullptr);
			setError(ESP_FAIL);
			return false;
		}

		esp_eth_config_t driverConfig = ETH_DEFAULT_CONFIG(mac, phy);
		auto result = esp_eth_driver_install(&driverConfig, &g_runtime.handle);
		if (result != ESP_OK) {
			destroyPartialDriver(mac, phy);
			setError(result);
			return false;
		}
		refreshMac();
		refreshHostnameFromMac();

		esp_netif_config_t netifConfig = ESP_NETIF_DEFAULT_ETH();
		g_runtime.netif = esp_netif_new(&netifConfig);
		if (g_runtime.netif == nullptr) {
			setError(ESP_ERR_NO_MEM);
			return false;
		}
		if (g_runtime.status.hostname[0] != '\0') {
			result = esp_netif_set_hostname(g_runtime.netif, g_runtime.status.hostname);
			if (result != ESP_OK) {
				setError(result);
				return false;
			}
		}

		g_runtime.glue = esp_eth_new_netif_glue(g_runtime.handle);
		if (g_runtime.glue == nullptr) {
			setError(ESP_ERR_NO_MEM);
			return false;
		}
		result = esp_netif_attach(g_runtime.netif, g_runtime.glue);
		if (result != ESP_OK) {
			setError(result);
			return false;
		}

		result = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &ethEventHandler, nullptr);
		if (result != ESP_OK) {
			setError(result);
			return false;
		}
		result = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &gotIpEventHandler, nullptr);
		if (result != ESP_OK) {
			setError(result);
			return false;
		}
		g_runtime.eventHandlersRegistered = true;

		g_runtime.status.initialized = true;
		clearError();
		return true;
#endif
	}

} // namespace

bool start(lexxos::platform::BoardDescriptor const& board) {
	if (!installDriver(board)) {
		return false;
	}
	if (g_runtime.status.started) {
		clearError();
		return true;
	}
	auto const result = esp_eth_start(g_runtime.handle);
	if (result != ESP_OK) {
		setError(result);
		return false;
	}
	g_runtime.status.started = true;
	clearError();
	return true;
}

bool stop() {
	if (!g_runtime.status.initialized || g_runtime.handle == nullptr) {
		return true;
	}
	if (!g_runtime.status.started) {
		return true;
	}
	auto const result = esp_eth_stop(g_runtime.handle);
	if (result != ESP_OK) {
		setError(result);
		return false;
	}
	g_runtime.status.started = false;
	clearIp();
	clearError();
	return true;
}

bool restart(lexxos::platform::BoardDescriptor const& board) {
	if (!stop()) {
		return false;
	}
	return start(board);
}

Status status() {
	if (g_runtime.status.initialized && g_runtime.netif != nullptr) {
		esp_netif_ip_info_t ipInfo{};
		if (esp_netif_get_ip_info(g_runtime.netif, &ipInfo) == ESP_OK && ipInfo.ip.addr != 0) {
			g_runtime.status.gotIp = true;
			esp_ip4addr_ntoa(&ipInfo.ip, g_runtime.status.ip, sizeof(g_runtime.status.ip));
			esp_ip4addr_ntoa(&ipInfo.netmask, g_runtime.status.netmask, sizeof(g_runtime.status.netmask));
			esp_ip4addr_ntoa(&ipInfo.gw, g_runtime.status.gateway, sizeof(g_runtime.status.gateway));
		}
		refreshMac();
		if (g_runtime.status.linkUp) {
			refreshLinkDetails();
		}
	}
	return g_runtime.status;
}

} // namespace lexxos::devices::espidf_ethernet

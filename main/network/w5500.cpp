#include "w5500.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

static const char *TAG_ETH = "w5500";

#ifndef W5500_USE_INT
#define W5500_USE_INT 1
#endif

W5500::W5500()
{}

void W5500::makeEthMacFromEfuse(uint8_t out_mac[6])
{
    uint8_t base_mac[6] = {0};
    esp_err_t err = esp_efuse_mac_get_default(base_mac);

    if (err != ESP_OK) {
        out_mac[0] = 0x02;
        out_mac[1] = 0x00;
        out_mac[2] = 0x00;
        out_mac[3] = 0x00;
        out_mac[4] = 0x00;
        out_mac[5] = 0x01;
        return;
    }

    memcpy(out_mac, base_mac, 6);
    out_mac[0] = (out_mac[0] & 0xFE) | 0x02;
    out_mac[5] ^= 0x01;
}

void W5500::setEthMac(esp_eth_handle_t eth_handle, const char *tag)
{
    uint8_t mac[6];
    makeEthMacFromEfuse(mac);
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, mac));

    ESP_LOGW(tag, "ETH MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void W5500::hwResetGpio(gpio_num_t rst)
{
    gpio_config_t io = {};
    io.intr_type = GPIO_INTR_DISABLE;
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = 1ULL << rst;
    ESP_ERROR_CHECK(gpio_config(&io));

    gpio_set_level(rst, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(rst, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
}

void W5500::onLinkUp()
{
    ESP_LOGW(TAG_ETH, "ETH link UP");
    if (m_hookLinkUp) {
        m_hookLinkUp();
    }
}

void W5500::onLinkDown()
{
    ESP_LOGW(TAG_ETH, "ETH link DOWN");
    if (m_hookLinkDown) {
        m_hookLinkDown();
    }
}

void W5500::onGotIp()
{
    if (m_hookGotIp) {
        m_hookGotIp();
    }
}

void W5500::ethEventHandlerTrampoline(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void) base;
    (void) data;
    W5500 *self = static_cast<W5500 *>(arg);
    if (self) {
        self->handleEthEvent(id);
    }
}

void W5500::ipEventHandlerTrampoline(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void) base;
    W5500 *self = static_cast<W5500 *>(arg);
    if (self) {
        self->handleIpEvent(id, data);
    }
}

void W5500::handleEthEvent(int32_t id)
{
    switch (id) {
    case ETHERNET_EVENT_CONNECTED:
        m_linkUp = true;
        onLinkUp();
        break;

    case ETHERNET_EVENT_DISCONNECTED:
        m_linkUp = false;
        m_hasIp = false;
        strncpy(m_ipAddr, "0.0.0.0", sizeof(m_ipAddr));
        m_ipAddr[sizeof(m_ipAddr) - 1] = '\0';
        onLinkDown();
        break;

    default:
        break;
    }
}

void W5500::handleIpEvent(int32_t id, void *data)
{
    if (id != IP_EVENT_ETH_GOT_IP) {
        return;
    }

    const ip_event_got_ip_t *event = static_cast<const ip_event_got_ip_t *>(data);

    snprintf(m_ipAddr, sizeof(m_ipAddr), IPSTR, IP2STR(&event->ip_info.ip));
    m_hasIp = true;

    ESP_LOGW(TAG_ETH, "ETH GOT IP: " IPSTR "  MASK: " IPSTR "  GW: " IPSTR, IP2STR(&event->ip_info.ip),
             IP2STR(&event->ip_info.netmask), IP2STR(&event->ip_info.gw));

    onGotIp();
}

esp_err_t W5500::earlySpiInit()
{
    if (m_inited) {
        return ESP_OK;
    }

    ESP_LOGW(TAG_ETH, "W5500::init start");

    hwResetGpio(m_pinRst);

    /* Create netif */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    m_ethNetif = esp_netif_new(&netif_cfg);
    if (!m_ethNetif) {
        ESP_LOGE(TAG_ETH, "esp_netif_new(ETH) failed");
        return ESP_FAIL;
    }

    /* SPI bus */
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = m_pinMosi;
    buscfg.miso_io_num = m_pinMiso;
    buscfg.sclk_io_num = m_pinSclk;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;

    const spi_host_device_t spi_host = SPI2_HOST;

    esp_err_t err = spi_bus_initialize(spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_ETH, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    spi_device_interface_config_t spi_devcfg = {};
    spi_devcfg.command_bits = 16;
    spi_devcfg.address_bits = 8;
    spi_devcfg.mode = 0;
    spi_devcfg.clock_speed_hz = 2 * 1000 * 1000;
    spi_devcfg.spics_io_num = m_pinCs;
    spi_devcfg.queue_size = 20;

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_host, &spi_devcfg);

#if W5500_USE_INT
    w5500_config.int_gpio_num = m_pinInt;
#else
    w5500_config.int_gpio_num = -1;
    w5500_config.poll_period_ms = 1;
#endif

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (!mac) {
        ESP_LOGE(TAG_ETH, "esp_eth_mac_new_w5500 failed");
        return ESP_FAIL;
    }

    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!phy) {
        ESP_LOGE(TAG_ETH, "esp_eth_phy_new_w5500 failed");
        mac->del(mac);
        return ESP_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);

    err = esp_eth_driver_install(&eth_config, &m_ethHandle);
    if (err != ESP_OK || !m_ethHandle) {
        ESP_LOGE(TAG_ETH, "esp_eth_driver_install failed: %s", esp_err_to_name(err));
        phy->del(phy);
        mac->del(mac);
        return (err != ESP_OK) ? err : ESP_FAIL;
    }

    setEthMac(m_ethHandle, TAG_ETH);

    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(m_ethHandle);
    if (!glue) {
        ESP_LOGE(TAG_ETH, "esp_eth_new_netif_glue returned NULL");
        esp_eth_driver_uninstall(m_ethHandle);
        m_ethHandle = nullptr;
        return ESP_FAIL;
    }

    err = esp_netif_attach(m_ethNetif, glue);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_ETH, "esp_netif_attach failed: %s", esp_err_to_name(err));
        esp_eth_driver_uninstall(m_ethHandle);
        m_ethHandle = nullptr;
        return err;
    }

    return ESP_OK;
}

esp_err_t W5500::init()
{
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &W5500::ethEventHandlerTrampoline, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &W5500::ipEventHandlerTrampoline, this));

    esp_err_t err = esp_eth_start(m_ethHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_ETH, "esp_eth_start failed: %s", esp_err_to_name(err));
        return err;
    }

    m_inited = true;
    ESP_LOGW(TAG_ETH, "W5500 init done");
    return ESP_OK;
}

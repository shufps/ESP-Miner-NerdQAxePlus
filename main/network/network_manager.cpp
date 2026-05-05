#include <string.h>
#include "network_manager.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

static const char *TAG_NET = "netmgr";

NetworkManager *NetworkManager::s_instance = nullptr;

NetworkManager::NetworkManager()
{}

esp_err_t NetworkManager::init()
{
    if (m_inited) {
        return ESP_OK;
    }

    /* Ensure netif + default event loop exist (idempotent) */
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    if (!m_eg) {
        m_eg = xEventGroupCreate();
        if (!m_eg) {
            return ESP_FAIL;
        }
    }

    s_instance = this;

    /* Wire WiFi hooks */
    wifi_set_hook_got_ip(&NetworkManager::wifiGotIpHook);
    wifi_set_hook_disconnected(&NetworkManager::wifiDisconnectedHook);

    /* Wire ETH hooks */
    m_eth.setHookGotIp(&NetworkManager::ethGotIpHook);
    m_eth.setHookLinkDown(&NetworkManager::ethLinkDownHook);

    m_inited = true;
    return ESP_OK;
}

esp_err_t NetworkManager::start(const char *wifi_ssid, const char *wifi_pass, const char *hostname,
                                bool hasEth)
{
    esp_err_t err = init();
    if (err != ESP_OK) {
        return err;
    }

    /* Start WiFi (APSTA + STA) */
    m_wifiStaNetif = wifi_init(wifi_ssid, wifi_pass, hostname);

    /* Start Ethernet only on boards that have it */
    if (hasEth) {
        err = m_eth.init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG_NET, "ETH init failed: %s", esp_err_to_name(err));
        }
    }

    return ESP_OK;
}

EventBits_t NetworkManager::waitAnyIpMs(TickType_t ticks)
{
    if (!m_eg) {
        return 0;
    }

    return xEventGroupWaitBits(m_eg, NET_WIFI_IP | NET_ETH_IP, pdFALSE, pdFALSE, ticks);
}

void NetworkManager::updateDefaultRoute()
{
    /* Prefer ETH if it has an IP, else WiFi if it has an IP */
    esp_netif_t *newPreferred = nullptr;

    if (m_ethHasIp && m_eth.getNetif()) {
        newPreferred = m_eth.getNetif();
    } else if (m_wifiHasIp && m_wifiStaNetif) {
        newPreferred = m_wifiStaNetif;
    }

    bool preferredChanged = (newPreferred != m_preferredNetif);
    m_preferredNetif = newPreferred;

    if (m_preferredNetif) {
        esp_err_t err = esp_netif_set_default_netif(m_preferredNetif);
        if (err != ESP_OK) {
            ESP_LOGW(TAG_NET, "esp_netif_set_default_netif failed: %s", esp_err_to_name(err));
        }
    }

    bool nowAny = (m_wifiHasIp || m_ethHasIp);
    bool anyRising = (!m_anyIp && nowAny);
    m_anyIp = nowAny;

    if (anyRising && m_hookAnyIp) {
        m_hookAnyIp();
    }

    if (preferredChanged && m_hookPreferredChanged) {
        m_hookPreferredChanged();
    }
}

void NetworkManager::shutdownApOnce()
{
    if (!m_apShutdownDone) {
        ESP_LOGI(TAG_NET, "First IP acquired -> AP permanently off");
        wifi_softap_off();
        m_apShutdownDone = true;
    }
}

void NetworkManager::onWifiGotIp()
{
    m_wifiHasIp = true;
    if (m_eg) {
        xEventGroupSetBits(m_eg, NET_WIFI_IP);
    }
    shutdownApOnce();
    updateDefaultRoute();
}

void NetworkManager::onWifiDisconnected()
{
    m_wifiHasIp = false;
    if (m_eg) {
        xEventGroupClearBits(m_eg, NET_WIFI_IP);
    }
    updateDefaultRoute();
}

void NetworkManager::onEthGotIp()
{
    m_ethHasIp = true;
    if (m_eg) {
        xEventGroupSetBits(m_eg, NET_ETH_IP);
    }

    shutdownApOnce();

    // ETH has an IP -> shut down WiFi STA (no longer needed)
    if (!m_wifiDisabledBecauseEth) {
        ESP_LOGI(TAG_NET, "ETH has IP -> stopping WiFi");
        esp_wifi_disconnect();
        esp_wifi_stop();
        m_wifiHasIp = false;
        m_wifiDisabledBecauseEth = true;
    }

    updateDefaultRoute();
}


void NetworkManager::onEthLinkDown()
{
    m_ethHasIp = false;
    if (m_eg) {
        xEventGroupClearBits(m_eg, NET_ETH_IP);
    }

    // ETH gone -> restart WiFi so we have fallback connectivity
    if (m_wifiDisabledBecauseEth) {
        ESP_LOGI(TAG_NET, "ETH link down -> restarting WiFi");
        esp_wifi_start();
        m_wifiDisabledBecauseEth = false;
    }

    updateDefaultRoute();
}


bool NetworkManager::getPreferredIpAddr(char *buf, size_t buf_len, bool* isEth) const
{
    if (!buf || buf_len == 0) {
        return false;
    }

    // Default: no connection
    strncpy(buf, "0.0.0.0", buf_len);
    buf[buf_len - 1] = '\0';

    // Prefer Ethernet if it has an IP
    if (isEth) {
        *isEth = false;
    }
    if (m_ethHasIp && m_eth.hasIp()) {
        if (isEth) {
            *isEth = true;
        }
        const char *ip = m_eth.getIpStr();
        if (ip && ip[0] != '\0') {
            strncpy(buf, ip, buf_len);
            buf[buf_len - 1] = '\0';
            return true;
        }
        return false;
    }

    // Fallback to WiFi
    if (m_wifiHasIp) {
        char tmp[20] = {0};
        bool ok = connect_get_ip_addr(tmp, sizeof(tmp));
        if (ok && tmp[0] != '\0') {
            strncpy(buf, tmp, buf_len);
            buf[buf_len - 1] = '\0';
            return true;
        }
        return false;
    }

    return false;
}
/* ---- static hooks ---- */

void NetworkManager::wifiGotIpHook()
{
    if (s_instance) {
        s_instance->onWifiGotIp();
    }
}

void NetworkManager::wifiDisconnectedHook()
{
    if (s_instance) {
        s_instance->onWifiDisconnected();
    }
}

void NetworkManager::ethGotIpHook()
{
    if (s_instance) {
        s_instance->onEthGotIp();
    }
}

void NetworkManager::ethLinkDownHook()
{
    if (s_instance) {
        s_instance->onEthLinkDown();
    }
}

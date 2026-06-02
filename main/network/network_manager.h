#pragma once

#include "esp_err.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "connect.h"
#include "w5500.h"

/* Simple hook signature */
typedef void (*NetworkHookFn)();

class NetworkManager {
  public:
    NetworkManager();

    /* Safe to call multiple times */
    esp_err_t init();

    /* Starts WiFi and optionally ETH (pass board->hasEthernet()) */
    esp_err_t start(const char *wifi_ssid, const char *wifi_pass, const char *hostname,
                    bool hasEth = false);

    /* Wait until either WiFi or ETH has an IP */
    EventBits_t waitAnyIpMs(TickType_t ticks);

    bool getPreferredIpAddr(char *buf, size_t buf_len, bool* isEth) const;

    bool hasWifiIp() const
    {
        return m_wifiHasIp;
    }
    bool hasEthIp() const
    {
        return m_ethHasIp;
    }

    esp_netif_t *getWifiStaNetif() const
    {
        return m_wifiStaNetif;
    }
    esp_netif_t *getEthNetif() const
    {
        return m_eth.getNetif();
    }

    esp_netif_t *getPreferredNetif() const
    {
        return m_preferredNetif;
    }

    void setHookPreferredChanged(NetworkHookFn fn)
    {
        m_hookPreferredChanged = fn;
    }
    void setHookAnyIp(NetworkHookFn fn)
    {
        m_hookAnyIp = fn;
    }

    bool isApActive() const {
        return !m_apShutdownDone;
    }

    void earlyEthSpiInit()
    {
        m_eth.earlySpiInit();
    }

    void shutdownApOnce();

  private:
    void updateDefaultRoute();

    void onWifiGotIp();
    void onWifiDisconnected();

    void onEthGotIp();
    void onEthLinkDown();

    static void wifiGotIpHook();
    static void wifiDisconnectedHook();

    static void ethGotIpHook();
    static void ethLinkDownHook();

  private:
    static NetworkManager *s_instance;

    bool m_inited = false;

    EventGroupHandle_t m_eg = nullptr;
    static constexpr EventBits_t NET_WIFI_IP = BIT0;
    static constexpr EventBits_t NET_ETH_IP = BIT1;

    bool m_wifiHasIp = false;
    bool m_ethHasIp = false;
    bool m_anyIp = false;

    esp_netif_t *m_wifiStaNetif = nullptr;
    esp_netif_t *m_preferredNetif = nullptr;

    W5500 m_eth;

    NetworkHookFn m_hookPreferredChanged = nullptr;
    NetworkHookFn m_hookAnyIp = nullptr;

    bool m_apShutdownDone = false;
    bool m_wifiDisabledBecauseEth = false;
};

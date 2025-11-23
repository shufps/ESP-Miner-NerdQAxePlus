#pragma once

#include "ArduinoJson.h"
#include "nvs_config.h"
#include "psram_allocator.h"

class StratumManager;
class StratumTask;

class StratumConfig {
    //friend StratumManager;
    //friend StratumTask;
  protected:
    bool m_primary = false;
    char *m_host = nullptr;
    int m_port = 0;
    char *m_user = nullptr;
    char *m_password = nullptr;
    bool m_enonceSub = false;

  public:
    StratumConfig() {}

    ~StratumConfig()
    {
        safe_free(m_host);
        safe_free(m_user);
        safe_free(m_password);
    }

    // Copy constructor
    StratumConfig(const StratumConfig &other)
    {
        m_primary = other.m_primary;
        m_port = other.m_port;
        m_enonceSub = other.m_enonceSub;

        m_host = other.m_host ? strdup(other.m_host) : nullptr;
        m_user = other.m_user ? strdup(other.m_user) : nullptr;
        m_password = other.m_password ? strdup(other.m_password) : nullptr;
    }

    // Copy assignment operator (deep copy)
    StratumConfig &operator=(const StratumConfig &other)
    {
        if (this == &other)
            return *this;

        safe_free(m_host);
        safe_free(m_user);
        safe_free(m_password);

        m_primary = other.m_primary;
        m_port = other.m_port;
        m_enonceSub = other.m_enonceSub;

        m_host = other.m_host ? strdup(other.m_host) : nullptr;
        m_user = other.m_user ? strdup(other.m_user) : nullptr;
        m_password = other.m_password ? strdup(other.m_password) : nullptr;

        return *this;
    }

    bool isPrimary()
    {
        return m_primary;
    }

    const char *getHost()
    {
        return m_host;
    }

    int getPort()
    {
        return m_port;
    }

    const char *getUser()
    {
        return m_user;
    }

    const char *getPassword()
    {
        return m_password;
    }

    bool isEnonceSubscribeEnabled()
    {
        return m_enonceSub;
    }

    bool isEqual(const StratumConfig &other);

    static void toLog(const StratumConfig &cfg, const char* prefix="");

    static StratumConfig read(int pool);
    static StratumConfig readPrimary();
    static StratumConfig readFallback();
};


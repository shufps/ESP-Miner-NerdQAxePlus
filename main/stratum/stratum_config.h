#pragma once

#include "ArduinoJson.h"
#include "nvs_config.h"
#include "psram_allocator.h"

class StratumManager;
class StratumTask;

class StratumConfig {
  protected:
    bool m_primary = false;
    char *m_host = nullptr;
    int m_port = 0;
    char *m_user = nullptr;
    char *m_password = nullptr;
    bool m_enonceSub = false;
    bool m_tls = false;

  public:
    StratumConfig(int pool);

    ~StratumConfig()
    {
        safe_free(m_host);
        safe_free(m_user);
        safe_free(m_password);
    }

    void copyInto(StratumConfig *dst);
    bool reload();

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

    bool isTLS() {
        return m_tls;
    }

    //static void toLog(const StratumConfig &cfg, const char* prefix="");
};


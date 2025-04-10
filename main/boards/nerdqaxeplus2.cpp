#include "board.h"
#include "nerdqaxeplus2.h"

static const char* TAG="nerdqaxeplus2";

NerdQaxePlus2::NerdQaxePlus2() : NerdQaxePlus() {
    m_deviceModel = "NerdQAxe++";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 4;
    m_numPhases = 3;
    m_imax = m_numPhases * 30;
    m_ifault = (float) (m_imax - 5);

    m_asicJobIntervalMs = 500;
    m_defaultAsicFrequency = m_asicFrequency = 600;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1150; // default voltage
    m_initVoltageMillis = 1200;

    m_pidSettings.targetTemp = 55;
    m_pidSettings.p = 600;  //  6.00
    m_pidSettings.i = 10;   //  0.10
    m_pidSettings.d = 1000; // 10.00

    m_maxPin = 100.0;
    m_minPin = 52.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;

#ifdef NERDQAXEPLUS2
    m_theme = new ThemeNerdqaxeplus2();
#endif
    m_asics = new BM1370();
}

float NerdQaxePlus2::getTemperature(int index) {
    float temp = NerdQaxePlus::getTemperature(index);
    if (!temp) {
        return 0.0;
    }
    // we can't read the real chip temps but this should be about right
    return temp + 10.0f; // offset of 10°C
}

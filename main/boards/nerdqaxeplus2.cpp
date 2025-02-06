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
    m_asicFrequency = 600.0;
    m_asicVoltage = 1.15; // default voltage
    m_initVoltage = 1.20;

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

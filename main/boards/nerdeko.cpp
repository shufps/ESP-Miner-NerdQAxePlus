#include "nerdeko.h"
#include "board.h"
#include "nerdqaxeplus2.h"
#include "./drivers/TPS53667.h"

static const char *TAG = "nerdeko";

NerdEko::NerdEko() : NerdQaxePlus2()
{
    m_deviceModel = "NerdEKO";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 12;
    m_numPhases = 6;
    m_imax = 240; // R = 6000 / (num_phases * max_current) = 24K9
    m_ifault = 235.0;

    // use m_asicVoltage for init
    m_initVoltageMillis = 0;

    m_maxPin = 350.0;
    m_minPin = 30.0;
    m_maxVin = 13.0;
    m_minVin = 8.0;

    m_asicMaxDifficulty = 8192;
    m_asicMinDifficulty = 2048;

#ifdef NERDEKO
    m_theme = new ThemeNerdeko();
#endif

    m_tps = new TPS53667();
}

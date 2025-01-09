#include "board.h"
#include "nerdeko.h"
#include "nerdqaxeplus2.h"

static const char* TAG="nerdeko";

NerdEko::NerdEko() : NerdQaxePlus2() {
    m_deviceModel = "NerdEko";
    m_asicModel = "BM1370";
    m_asicCount = 12;
    m_numPhases = 6;
    m_imax = 180;
    m_ifault = 170.0;

    // use m_asicVoltage for init
    m_initVoltage = 0.0;

    m_maxPin = 300.0;
    m_minPin = 100.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;

    m_asicMaxDifficulty = 8192;
    m_asicMinDifficulty = 2048;

#ifdef NERDEKO
    m_theme = new ThemeNerdoctaxegamma();
#endif
}

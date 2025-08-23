#include "board.h"
#include "nerdoctaxeplus.h"

static const char* TAG="nerdoctaxe+";

NerdOctaxePlus::NerdOctaxePlus() : NerdQaxePlus() {
    m_deviceModel = "NerdOCTAXE+";
    m_miningAgent = m_deviceModel;
    m_asicCount = 8;
    m_numPhases = 3;
    m_imax = m_numPhases * 30;
    m_ifault = (float) (m_imax - 5);

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;

    m_maxPin = 130.0;
    m_minPin = 70.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;

#ifdef NERDOCTAXEPLUS
    m_theme = new ThemeNerdoctaxeplus();
#endif

    m_swarmColorName = "#11d51e"; // green
}

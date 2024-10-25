#include "board.h"
#include "nerdoctaxegamma.h"
#include "nerdqaxeplus2.h"

static const char* TAG="nerdqaxeplus2";

NerdOctaxeGamma::NerdOctaxeGamma() : NerdQaxePlus2() {
    m_deviceModel = "NerdOCTAXEGamma";
    m_asicModel = "BM1370";
    m_asicCount = 8;
    m_numPhases = 4;
    m_imax = 180;

    // use m_asicVoltage for init
    m_initVoltage = 0.0;

    m_maxPin = 200.0;
    m_minPin = 100.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;

#ifdef NERDOCTAXEGAMMA
    m_theme = new ThemeNerdoctaxegamma();
#endif
}

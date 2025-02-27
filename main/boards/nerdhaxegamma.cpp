#include "board.h"
#include "nerdhaxegamma.h"
#include "nerdqaxeplus2.h"

static const char* TAG="nerdhaxegamma";

NerdHaxeGamma::NerdHaxeGamma() : NerdQaxePlus2() {
    m_deviceModel = "NerdHaxe-γ";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 6;
    m_numPhases = 4;
    m_imax = 120;
    m_ifault = 105.0;

    // use m_asicVoltage for init
    m_initVoltage = 0.0;

    m_params.maxPin = 150.0;
    m_params.minPin = 75.0;
    m_params.maxVin = 13.0;
    m_params.minVin = 11.0;

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;

#ifdef NERDHAXEGAMMA
    m_theme = new ThemeNerdhaxegamma();
#endif
}

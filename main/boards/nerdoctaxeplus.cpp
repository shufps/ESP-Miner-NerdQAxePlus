#include "board.h"
#include "nerdoctaxeplus.h"

static const char* TAG="nerdoctaxe+";

NerdOctaxePlus::NerdOctaxePlus() : NerdQaxePlus() {
    m_deviceModel = "NerdOCTAXE+";
    m_asicCount = 8;
    m_numPhases = 3;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;

#ifdef NERDOCTAXEPLUS
    m_theme = new ThemeNerdoctaxeplus();
#endif
}

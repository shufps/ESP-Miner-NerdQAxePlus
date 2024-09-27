#include "board.h"
#include "nerdoctaxeplus.h"

static const char* TAG="nerdoctaxe+";

NerdOctaxePlus::NerdOctaxePlus() : NerdQaxePlus() {
    m_deviceModel = "NerdOCTAXE+";
    m_asicCount = 8;
    m_numPhases = 3;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;

    m_theme = new Theme();
    m_theme->ui_img_btcscreen = &ui_img_nerdoctaxeplus_btcscreen_png;
    m_theme->ui_img_initscreen = &ui_img_nerdoctaxeplus_initscreen2_png;
    m_theme->ui_img_miningscreen = &ui_img_nerdoctaxeplus_miningscreen2_png;
    m_theme->ui_img_portalscreen = &ui_img_nerdoctaxeplus_portalscreen_png;
    m_theme->ui_img_settingscreen = &ui_img_nerdoctaxeplus_settingsscreen_png;
    m_theme->ui_img_splashscreen = &ui_img_nerdoctaxeplus_splashscreen2_png;
}

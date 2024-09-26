#include "board.h"
#include "nerdoctaxeplus.h"

static const char* TAG="nerdoctaxe+";

NerdOctaxePlus::NerdOctaxePlus() : NerdQaxePlus() {
    device_model = "NerdOCTAXE+";
    asic_count = 8;
    num_tps_phases = 3;

    asic_min_difficulty = 512;
    asic_max_difficulty = 2048;

    theme = new Theme();
    theme->ui_img_btcscreen = &ui_img_nerdoctaxeplus_btcscreen_png;
    theme->ui_img_initscreen = &ui_img_nerdoctaxeplus_initscreen2_png;
    theme->ui_img_miningscreen = &ui_img_nerdoctaxeplus_miningscreen2_png;
    theme->ui_img_portalscreen = &ui_img_nerdoctaxeplus_portalscreen_png;
    theme->ui_img_settingscreen = &ui_img_nerdoctaxeplus_settingsscreen_png;
    theme->ui_img_splashscreen = &ui_img_nerdoctaxeplus_splashscreen2_png;
}

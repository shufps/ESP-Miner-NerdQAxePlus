#include "board.h"
#include "nerdoctaxeplus.h"

static const char* TAG="nerdoctaxe+";

NerdOctaxePlus::NerdOctaxePlus() : NerdQaxePlus() {
    this->device_model = "NerdOCTAXE+";
    this->asic_count = 8;

    this->ui_img_btcscreen = &ui_img_nerdoctaxeplus_btcscreen_png;
    this->ui_img_initscreen = &ui_img_nerdoctaxeplus_initscreen2_png;
    this->ui_img_miningscreen = &ui_img_nerdoctaxeplus_miningscreen2_png;
    this->ui_img_portalscreen = &ui_img_nerdoctaxeplus_portalscreen_png;
    this->ui_img_settingscreen = &ui_img_nerdoctaxeplus_settingsscreen_png;
    this->ui_img_splashscreen = &ui_img_nerdoctaxeplus_splashscreen2_png;

}

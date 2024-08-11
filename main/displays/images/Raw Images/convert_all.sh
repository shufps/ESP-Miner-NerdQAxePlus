#!/bin/bash

set -e

python3 convert_single.py InitScreen2.png ../ui_img_initscreen2_png.c
python3 convert_single.py MiningScreen2.png ../ui_img_miningscreen2_png.c
python3 convert_single.py PortalScreen.png ../ui_img_portalscreen_png.c
python3 convert_single.py BTCScreen.png ../ui_img_btcscreen_png.c
python3 convert_single.py SettingsScreen.png ../ui_img_settingsscreen_png.c
python3 convert_single.py SplashScreen2.png ../ui_img_splashscreen2_png.c



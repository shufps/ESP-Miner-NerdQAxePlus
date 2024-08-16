#!/bin/bash

set -e

python3 convert_single.py ../Raw\ Images/InitScreen2.png ../ui_img_initscreen2_png.c
python3 convert_single.py ../Raw\ Images/MiningScreen2.png ../ui_img_miningscreen2_png.c
python3 convert_single.py ../Raw\ Images/PortalScreen.png ../ui_img_portalscreen_png.c
python3 convert_single.py ../Raw\ Images/BTCScreen.png ../ui_img_btcscreen_png.c
python3 convert_single.py ../Raw\ Images/SettingsScreen.png ../ui_img_settingsscreen_png.c
python3 convert_single.py ../Raw\ Images/SplashScreen2.png ../ui_img_splashscreen2_png.c



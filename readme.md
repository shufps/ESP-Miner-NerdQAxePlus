[![](https://dcbadge.vercel.app/api/server/3E8ca2dkcC)](https://discord.gg/3E8ca2dkcC)

# ESP-Miner-Nerdaxe version

| Supported Targets | ESP32-S3              |
| ----------------- | --------------------- |
| Required Platform | ESP-IDF v4.4.6        |
| ----------------- | --------------------- |

This is a forked version of ESP-miner, the original firmware of Bitaxe project developed by @skot/ESP-Miner, @ben and @jhonny.
The current fork supports LVGL library with an UI that works with 8bit parallel screen over `TTGO-TdiplayS3` board.

This features unfortunatelly can't be added to the original project because requires specific ESP-IDF version to be built.


## How to flash/update firmware

#### Online Flashtool [Recommended]

Easyiest way to flash firmware. Build your own miner using the folowing firwmare flash tool:

1. Get a TTGO T-display S3 
1. Get a NerdAxe board
1. Go to flasher online tool: https://flasher.bitronics.store/ (recommend via Google Chrome incognito mode)

#### Bitaxetool

The bitaxetool includes all necessary library for flashing the binary file to the Bitaxe Hardware.

The bitaxetool requires a config.cvs preloaded file and the appropiate firmware.bin file in it's executed directory.

3. Flash with the bitaxetool

```
bitaxetool --config ./config.cvs --firmware ./esp-miner-factory-nerd101-v2.1.4.bin
```

## How to build firmware


Install bitaxetool from pip. pip is included with Python 3.4 but if you need to install it check <https://pip.pypa.io/en/stable/installation/>

```
pip install --upgrade bitaxetool
```

## Preconfiguration

Starting with v2.0.0, the ESP-Miner firmware requires some basic manufacturing data to be flashed in the NVS partition.

1. Download the esp-miner-factory-v2.0.3.bin file from the release tab.
   Click [here](https://github.com/skot/ESP-Miner/releases) for the release tab

2. Copy `config.cvs.example` to `config.cvs` and modify `asicfrequency`, `asicvoltage`, `asicmodel`, `devicemodel`, and `boardversion`

The following are recommendations but it is necessary that you do have all values in your `config.cvs`file to flash properly.

- recommended values for the NerdAxe 1366 (ultra)

  ```
  key,type,encoding,value
  main,namespace,,
  asicfrequency,data,u16,485
  asicvoltage,data,u16,1200
  asicmodel,data,string,BM1366
  devicemodel,data,string,ultra
  boardversion,data,string,101
  ```

## API
Nerdaxe uses same bitaxe API funcitons.

For more details take a look at `main/http_server/http_server.c`.

Things that can be done are:
  
  - Get System Info
  - Get Swarm Info
  - Update Swarm
  - Swarm Options
  - System Restart Action
  - Update System Settings Action
  - System Options
  - Update OTA Firmware
  - Update OTA WWW
  - WebSocket

Some API examples in curl:
  ```bash
  # Get system information
  curl http://YOUR-BITAXE-IP/api/system/info
  ```
  ```bash
  # Get swarm information
  curl http://YOUR-BITAXE-IP/api/swarm/info
  ```
  ```bash
  # System restart action
  curl -X POST http://YOUR-BITAXE-IP/api/system/restart
  ```
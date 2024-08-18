[![](https://dcbadge.vercel.app/api/server/3E8ca2dkcC)](https://discord.gg/3E8ca2dkcC)

# ESP-Miner-Nerdaxe version

| Supported Targets | ESP32-S3              |
| ----------------- | --------------------- |
| Old Platform      | ~~ESP-IDF v4.4.6~~        |
| ----------------- | --------------------- |
| Required Platform | >= ESP-IDF v5.2.X       | Update!
| ----------------- | --------------------- |

This is a forked version from the NerdAxe miner that was modified for using on the NerdQaxe+.

Both are using ESP-miner as its base - the original firmware of Bitaxe project developed by @skot/ESP-Miner, @ben and @jhonny.
The current fork supports LVGL library with an UI that works with 8bit parallel screen over `TTGO-TdiplayS3` board.



## How to flash/update firmware


#### Bitaxetool

The bitaxetool includes all necessary library for flashing the binary file to the Bitaxe Hardware.

The bitaxetool requires a config.cvs preloaded file and the appropiate firmware.bin file in it's executed directory.

3. Flash with the bitaxetool

```
bitaxetool --config ./config.cvs --firmware ./esp-miner-factory-nerdqaxe+.bin
```


## How to build firmware

### Using Docker

Docker containers allow to use the toolchain without installing anything on the system.

#### 1. First build the docker container

```bash
cd docker
./build_docker.sh
```

#### 2. How to use it

There are several scripts in the `docker` directory but what is most flexible is to just start the container as bash via

```bash
./docker/idf-shell.sh
```

You will get a new terminal that supports tools like:
- `idf.py`
- `bitaxetool`
- `esptool.py`
- `nvs_partition_gen.py`

The current repository will be mounted to `/home/builder/project` with `uid:gid = 1000:1000` (like the main user on *buntu/Mint)

#### 3. Compiling & Flashing using the shell

This are all manual steps:

```bash
# set target and build the binaries
idf.py set-target esp32s3
idf.py build

# creat config.bin nvm partition from config.cvs
nvs_partition_gen.py generate config.cvs config.bin 12288

# merge all partitions including config into a single binary
./merge_bin_with_config.sh nerdqaxe+.bin

# flash using esptool
esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  --before=default_reset --after=hard_reset write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 nerdqaxe+.bin
```

##### 3.1. Compiling & Flashing using BitAxe tool

```bash
# set target and build the binaries
idf.py set-target esp32s3
idf.py build

bitaxetool --config config.cvs --firmware esp-miner-factory-nerdqaxe+.bin  -p /dev/ttyACM0
```


When done just `exit` the shell.


### Without Docker

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
  asicfrequency,data,u16,490
  asicvoltage,data,u16,1200
  asicmodel,data,string,BM1368
  devicemodel,data,string,nerdqaxe_plus
  boardversion,data,string,500
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

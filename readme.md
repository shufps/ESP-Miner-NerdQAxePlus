[![](https://dcbadge.vercel.app/api/server/3E8ca2dkcC)](https://discord.gg/3E8ca2dkcC)

# ESP-Miner-Nerdaxe version

| Supported Targets | ESP32-S3              |
| ----------------- | --------------------- |
| Required Platform | >= ESP-IDF v5.3.X       |
| ----------------- | --------------------- |

This is a forked version from the NerdAxe miner that was modified for using on the NerdQAxe+.

Credits to the devs:
- BitAxe devs on OSMU: @skot/ESP-Miner, @ben and @jhonny
- NerdAxe dev @BitMaker


## How to flash/update firmware

The newest releases are always here:

https://github.com/shufps/ESP-Miner-NerdQAxePlus/releases

#### Clone repository and prepare config

First you need to clone the repository and create a local copy of the config file:

```bash
# clone repository
git clone https://github.com/shufps/ESP-Miner-NerdQAxePlus

# change into the cloned repository
cd ESP-Miner-NerdQAxePlus

# copy the example config
cp config.cvs.example config.cvs
```

Then you can edit the fields like `stratumurl` and so on.

Please note that this fields will be removed soon because they are not used anymore:

- `devicemodel`
- `boardversion`
- `asicmodel`


#### Bitaxetool

After the changes on the `config.cvs` files are done, you use the `bitaxetool` to flash factory binary and the config onto the device.

To switch it into bootload mode, reset the device with presset `boot` button.

```
bitaxetool --config ./config.cvs --firmware esp-miner-factory-NERDQAXEPLUS-v1.0.10.bin

```


## How to build firmware

### Using Docker

Docker containers allow to use the toolchain without installing `esp-idf` or `Node 20.x` on the system.

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

You will get a new terminal that provides tools like:
- `idf.py`
- `bitaxetool`
- `esptool.py`
- `nvs_partition_gen.py`

The current repository will be mounted to `/home/builder/project`.

The default `builder` user has `uid:gid = 1000:1000` (like the main user on *buntu/Mint)

#### 3. Compiling & Flashing using the shell

#### 3.1. Just flashing with dockered `bitaxetool` with factory binary

(no `idf-shell.sh` version)

```bash
./docker/bitaxetool.sh --config config.cvs --firmware esp-miner-factory-NERDQAXEPLUS-v1.0.10.bin -p /dev/ttyACM0
```

##### 3.2. Compiling & Flashing using BitAxe tool

(inside of `idf-shell.sh`)

```bash
# start idf-shell
./docker/idf-shell.sh

# set target and build the binaries
idf.py set-target esp32s3
idf.py build

# merge all partitions including config into a single binary
./merge_bin.sh nerdqaxe+.bin

bitaxetool --config config.cvs --firmware esp-miner-factory-nerdqaxe+.bin  -p /dev/ttyACM0
```

#### 3.3. All manual steps for building and flashing

(inside of `idf-shell.sh`)

```bash
# start idf-shell
./docker/idf-shell.sh

# set target and build the binaries
idf.py set-target esp32s3

# optional if you want to change the sdkconfig
idf.py menuconfig

# build the binaries
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



When done just `exit` the shell.


### Without Docker

Install bitaxetool from pip. pip is included with Python 3.4 but if you need to install it check <https://pip.pypa.io/en/stable/installation/>

```
pip install --upgrade bitaxetool
```

## Grafana Monitoring

The NerdQaxe+ firmware supports Influx and the repository provides an installation with Grafana dashboard that can be started with a few bash commands: https://github.com/shufps/ESP-Miner-NerdQAxePlus/tree/master/monitoring


## API
Nerdaxe uses the same bitaxe API functions.

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

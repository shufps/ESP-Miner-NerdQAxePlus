[![](https://dcbadge.vercel.app/api/server/3E8ca2dkcC)](https://discord.gg/3E8ca2dkcC)

# ESP-Miner-Nerdaxe version

| Supported Targets | ESP32-S3              |
| ----------------- | --------------------- |
| Required Platform | >= ESP-IDF v5.3.X       |
| ----------------- | --------------------- |

This is a forked version from the NerdAxe miner that was modified for using on the [NerdQAxe+](https://github.com/shufps/qaxe).

Credits to the devs:
- BitAxe devs on OSMU: @skot/ESP-Miner, @ben and @jhonny
- NerdAxe dev @BitMaker


## How to flash/update firmware

The newest releases are always here:

https://github.com/shufps/ESP-Miner-NerdQAxePlus/releases

### Recommended Method: The Webflasher

The [Webflasher](https://shufps.github.io/nerdqaxe-web-flasher/) (modified fork of the great [Bitaxe Webflasher](https://github.com/bitaxeorg/bitaxe-web-flasher) by [Wantclue](https://github.com/WantClue)) is the easiest method of updating all Nerd*axe variants.

[<img src="https://github.com/user-attachments/assets/4168f23a-bfe7-4536-91e3-7af6df9a203a" style="border:5px solid red;width:200px">](https://shufps.github.io/nerdqaxe-web-flasher/)

It uses the official releases published on this repository and is always up-to-date.

### Other Methods

#### Clone repository and prepare config

First you need to clone the repository and create a local copy of the config file:

```bash
# clone repository
git clone --recursive https://github.com/shufps/ESP-Miner-NerdQAxePlus

# change into the cloned repository
cd ESP-Miner-NerdQAxePlus

# copy the example config
cp config.cvs.example config.cvs
```

Then you can edit the fields like `stratumurl` and so on.

#### Bitaxetool

After the changes on the `config.cvs` files are done, you use the `bitaxetool` to flash factory binary and the config onto the device.

To switch it into bootload mode, reset the device with presset `boot` button.

```
bitaxetool --config ./config.cvs --firmware esp-miner-factory-NERDQAXEPLUS-v1.0.10.bin

```


## How to build firmware

### Using Docker

Docker containers allow to use the toolchain without installing `esp-idf` or `Node 20.x` on the system.

#### 0. TL;DR - `esp-miner.bin`, `www.bin`
```bash

# only once
cd docker
./build_docker.sh
cd ..

# only needed if you cloned without --recursive
git submodule update --init --recursive

export BOARD="NERDQAXEPLUS2"
./docker/idf.sh set-target esp32s3

# after each change on the source code
./docker/idf.sh build
```

Afterwards you will have a `esp-miner.bin` and `www.bin` in your `build` directory.


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

# set board
export BOARD="NERDQAXEPLUS2"

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

# set board
export BOARD="NERDQAXEPLUS2"

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

### Persistent panic core dumps

Firmware panic core dumps are stored in the existing 64K `coredump` flash partition. The dump uses ESP-IDF's ELF format and includes task registers and stacks, but not a full DRAM/heap capture. Each release also includes a board-specific `esp-miner-<board>.elf`; keep the ELF from the exact firmware build that produced the dump.

The System status page can download the last valid dump without erasing it. The download is deliberately user-initiated and uses the normal OTP/session protection when OTP is enabled. The filename includes the board, firmware version, and the crashing application's ELF SHA-256 from the dump itself to identify the matching build, even if firmware has since been updated. Decode the downloaded raw file with the exact matching board ELF:

```bash
esp-coredump --chip esp32s3 info_corefile --core coredump_<board>_<version>_<elf-sha256>_<timestamp>.bin --core-format raw esp-miner-<board>.elf
```

From the matching local build directory, ESP-IDF can retrieve and decode the dump directly:

```bash
idf.py -p <PORT> coredump-info
```

To retain the raw partition before decoding it with a downloaded release ELF:

```bash
parttool.py --port <PORT> read_partition --partition-name coredump --output coredump.bin
esp-coredump --chip esp32s3 info_corefile --core coredump.bin --core-format raw esp-miner-<board>.elf
```

Use a development device to validate the path. There is no production crash endpoint. For a temporary local test, add `ESP_ERROR_CHECK(ESP_FAIL);` in `app_main()` after normal startup tasks have been created, save that build's ELF, build and flash it, then remove the line. A successful decode identifies the crashed task, program counter, and backtrace.

Five task snapshots is the intended conservative limit for this partition. The ESP-IDF 5.3 revision currently used by CI defines this setting but does not enforce it in the core-dump writer, so a dump taken with many heavily used task stacks can still exceed 64K. Full-load validation is required when the toolchain changes.

Core dumps can contain credentials and other sensitive values present in task RAM. They are not uploaded automatically; do not casually share or attach a raw dump without reviewing how it will be handled.


### Without Docker

Install bitaxetool from pip. pip is included with Python 3.4 but if you need to install it check <https://pip.pypa.io/en/stable/installation/>

```
pip install --upgrade bitaxetool
```

## Grafana Monitoring

<img src="https://github.com/user-attachments/assets/3c485428-5e48-4761-9717-bd88579a747d" width="600px">

The NerdQaxe+ firmware supports Influx and the repository provides an installation with Grafana dashboard that can be started with a few bash commands: https://github.com/shufps/ESP-Miner-NerdQAxePlus/tree/master/monitoring



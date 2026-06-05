# mDNS / DNS-SD discovery (feat/mdns branch)

This branch makes the device discoverable on the LAN via mDNS, mirroring the
upstream AxeOS discovery surface
([bitaxeorg/ESP-Miner#1240](https://github.com/bitaxeorg/ESP-Miner/pull/1240)).

Once on WiFi (or Ethernet) the device advertises:

- Host record: `<hostname>.local` (hostname comes from NVS, default `Bitaxe`)
- Service: `_http._tcp` on port 80
- Subtype: `_axeos._sub._http._tcp` — filter for AxeOS devices only
- TXT records: `board`, `family`, `asic`, `asic_count`, `fw_version`

Init runs once from `setup_network()` right after the first IP is acquired.

## Build

Replace `NERDQAXEPLUS2` with your board: `NERDQAXEPLUS`, `NERDQAXEPLUS2`,
`NERDOCTAXEPLUS`, `NERDAXEGAMMA`, `NERDHAXEGAMMA`, `NERDEKO`, `NERDQX`,
`Q1370`, `Q1373` — see `.github/workflows/build.yml` for the full list.

```sh
git clone -b feat/mdns https://github.com/jankrejci/ESP-Miner-NerdQAxePlus.git
cd ESP-Miner-NerdQAxePlus
git submodule update --init --recursive
```

### Option A — official Docker builder

```sh
docker run --rm --user root -e BOARD=NERDQAXEPLUS2 \
  -v "$PWD":/home/builder/project \
  shufps/esp-idf-builder:0.0.1 \
  bash -c 'idf.py set-target esp32s3 && idf.py build'
```

### Option B — Nix flake (no Docker)

A `flake.nix` is included that pins ESP-IDF v5.5.2 + Node 22 via
[`mirrexagon/nixpkgs-esp-dev`](https://github.com/mirrexagon/nixpkgs-esp-dev).
Requires Nix with flakes enabled.

```sh
BOARD=NERDQAXEPLUS2 nix develop --command \
  bash -c 'idf.py set-target esp32s3 && idf.py build'
```

Artifacts land in `build/` (`esp-miner.bin`, `bootloader.bin`,
`partition-table.bin`, `www.bin`, `ota_data_initial.bin`).

## Flash

Plug the device via USB, find the serial port (`/dev/ttyACM0` on Linux,
`/dev/cu.usbmodem*` on macOS, `COMx` on Windows).

**Full flash** (overwrites bootloader, partitions, app, web UI, OTA data — NVS
at `0x9000` is preserved so WiFi/stratum settings survive):

```sh
esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0      build/bootloader/bootloader.bin \
  0x8000   build/partition_table/partition-table.bin \
  0x10000  build/esp-miner.bin \
  0x410000 build/www.bin \
  0xf10000 build/ota_data_initial.bin
```

**App-only update** (faster, only rewrites the firmware partition + OTA flag):

```sh
esptool.py --chip esp32s3 -p /dev/ttyACM0 write_flash \
  0x10000  build/esp-miner.bin \
  0xf10000 build/ota_data_initial.bin
```

If `idf.py` is on PATH, `idf.py -p /dev/ttyACM0 flash` does the equivalent of
the full-flash command.

## Verify

```sh
# resolve host record
avahi-resolve --name Bitaxe.local
ping Bitaxe.local

# browse all AxeOS devices on the LAN
avahi-browse -rt _axeos._sub._http._tcp

# read TXT records directly
dig +short @224.0.0.251 -p 5353 Bitaxe._http._tcp.local TXT
```

If `avahi-browse` returns nothing but the device is online, the WiFi router is
likely blocking IGMP/multicast between clients (common on public/guest SSIDs).
Discovery still works on regular LANs; until then you can hit the API directly
by IP: `curl http://<device-ip>/api/system/info`.

## Notes for porting

- The `espressif/mdns` managed component was already declared in
  `main/idf_component.yml` but unused before this branch
- TXT values are pulled from `Board::getDeviceModel/getAsicModel/getAsicCount/getVersion`
  and `esp_app_get_description()->version`
- The minimal port intentionally omits hostname conflict resolution,
  dynamic hostname update on settings change, and the separate-task init
  used in upstream PR #1240 — add them if needed

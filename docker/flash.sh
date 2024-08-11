#!/bin/bash

rpath="$( dirname "$( readlink -f "$0" )" )"
cd $rpath

#docker run --rm -it -v /dev:/dev --privileged -v "$rpath/..":/project esp-idf-builder /bin/bash -c 'idf.py flash -p /dev/ttyACM0; bitaxetool --config config-nerdqaxe+.csv --firmware ./build/esp-miner.bin  -p /dev/ttyACM0'
docker run --rm -it -v /dev:/dev --privileged -v "$rpath/..":/project esp-idf-builder /bin/bash -c 'bitaxetool --config config-nerdqaxe+.csv --firmware nerdqaxe+.bin  -p /dev/ttyACM0'

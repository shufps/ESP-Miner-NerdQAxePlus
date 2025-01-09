#!/bin/bash

rpath="$( dirname "$( readlink -f "$0" )" )"
cd $rpath

[ -z "$1" ] && {
	echo "usage: $0 <port>"
	echo
	echo "<port> is the USB port. For example /dev/ttyACM0"
	exit 0
}

./bitaxetool.sh --config config.cvs --firmware esp-miner-factory-nerdqaxe+.bin -p $1

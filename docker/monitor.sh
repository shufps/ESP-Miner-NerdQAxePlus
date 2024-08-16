#!/bin/bash

rpath="$( dirname "$( readlink -f "$0" )" )"
cd $rpath

./idf.sh monitor -p /dev/ttyACM0

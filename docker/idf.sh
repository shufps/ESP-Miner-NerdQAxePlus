#!/bin/bash

rpath="$( dirname "$( readlink -f "$0" )" )"
cd $rpath

docker run --rm -it -v /dev:/dev --privileged -v "$rpath/..":/home/builder/project esp-idf-builder /bin/bash -c "idf.py $@"

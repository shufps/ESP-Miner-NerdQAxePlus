#!/bin/bash

docker run --rm -it \
    -v "$(pwd)":/home/builder/project \
    -w /home/builder/project \
    esp-idf-builder:latest \
    idf.py "$@"

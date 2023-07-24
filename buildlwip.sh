#!/bin/bash

mkdir -p $1
CMAKELISTS_DIR=$(pwd)
cd $1
cmake ${CMAKELISTS_DIR}
make

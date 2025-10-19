#!/bin/bash
#
# https://github.com/alkalimc/QLogic-Everest-FastLinQ-Linux-Drivers
# Powered by alkali
# Copyright 2025- alkali. All Rights Reserved.
################################################################

KERNEL_VER=$(uname -r)
FASTLINQ_DIR=$(dirname "$(readlink -f "$0")")
N_PROC=$(nproc)

################################################################

if [ ! -d "/lib/modules/$KERNEL_VER/build" ]; then
    sudo apt install linux-headers-$KERNEL_VER -y
    if [ ! -d "/lib/modules/$KERNEL_VER/build" ]; then
        exit 1
    fi
fi

unset KERNEL_VER

cd $FASTLINQ_DIR
make clean -C $FASTLINQ_DIR -j$N_PROC
sudo make install -C $FASTLINQ_DIR -j$N_PROC

unset FASTLINQ_DIR
unset N_PROC
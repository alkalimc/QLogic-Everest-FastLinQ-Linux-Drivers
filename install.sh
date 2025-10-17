#!/bin/bash
#
# https://github.com/alkalimc/QLogic-Everest-FastLinQ-Linux-Drivers
# Powered by alkali
# Copyright 2025- alkali. All Rights Reserved.
################################################################

KERNEL_VER=$(uname -r)
SRC_DIR=/usr/src
LINUX_SRC=$SRC_DIR/linux-source-$(echo "$KERNEL_VER" | awk -F. '{print $1"."$2}')
LINUX_SRC_FILE=$LINUX_SRC.tar.xz
FASTLINQ_DIR=$(dirname "$(readlink -f "$0")")
N_PROC=$(nproc)

################################################################

if [ ! -d "$LINUX_SRC" ] && [ -f "$LINUX_SRC_FILE" ]; then
    sudo tar -xf $LINUX_SRC_FILE -C $SRC_DIR
elif [ ! -d "$LINUX_SRC" ]; then
    sudo apt install linux-source
    if [ ! -f "$LINUX_SRC_FILE" ]; then
        exit 1
    fi
    sudo tar -xf $LINUX_SRC_FILE -C $SRC_DIR
    if [ ! -d "$LINUX_SRC" ]; then
        exit 1
    fi
fi

unset SRC_DIR
unset LINUX_SRC
unset LINUX_SRC_FILE

if [ ! -d "/lib/modules/$KERNEL_VER/build" ]; then
    sudo apt install linux-headers-$KERNEL_VER -y
    if [ ! -d "/lib/modules/$KERNEL_VER/build" ]; then
        exit 1
    fi
fi

unset KERNEL_VER

cd $FASTLINQ_DIR
make clean -C $FASTLINQ_DIR -j$N_PROC
make -C $FASTLINQ_DIR -j$N_PROC
sudo make install -C $FASTLINQ_DIR -j$N_PROC

unset FASTLINQ_DIR
unset N_PROC
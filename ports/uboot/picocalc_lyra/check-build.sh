#!/bin/sh
# Run from the prepared U-Boot directory in the Linux build container.
set -eu
make CROSS_COMPILE=arm-linux-gnueabi- rk3506_defconfig
sed -i 's/# CONFIG_PICOCALC_LYRA_SPLASH is not set/CONFIG_PICOCALC_LYRA_SPLASH=y/' .config
make CROSS_COMPILE=arm-linux-gnueabi- olddefconfig
make CROSS_COMPILE=arm-linux-gnueabi- -j4 \
    board/rockchip/evb_rk3506/picocalc_splash.o arch/arm/mach-rockchip/board.o

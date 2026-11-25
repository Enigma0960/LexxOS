#!/bin/sh
# Invoked inside the Linux container by rk3506_build_uboot.py.
set -eu
export SOURCE_DATE_EPOCH=1700000000
export KBUILD_BUILD_USER=lexxos KBUILD_BUILD_HOST=builder
make CROSS_COMPILE=arm-linux-gnueabi- picocalc_rk3506_luckfox_defconfig
# Keep UART compatible with the existing Lyra bootstrap. The upstream SDK
# backlight symbol is absent here; our SPI splash uses PicoCalc's panel wiring.
sed -i 's/# CONFIG_PICOCALC_LYRA_SPLASH is not set/CONFIG_PICOCALC_LYRA_SPLASH=y/; s/CONFIG_BAUDRATE=115200/CONFIG_BAUDRATE=1500000/' .config
make CROSS_COMPILE=arm-linux-gnueabi- olddefconfig
# Two existing vendor diagnostics became warnings in newer GCC; retain all
# other -Werror checks, including diagnostics in our driver.
make CROSS_COMPILE=arm-linux-gnueabi- \
  'KCFLAGS=-Wno-error=format-overflow -Wno-error=maybe-uninitialized' \
  -j"${BUILD_JOBS:-4}" u-boot.bin u-boot.dtb tools
# Use the vendor generator, OP-TEE address from pinned RK3506TOS.ini.
export srctree=.
bash arch/arm/mach-rockchip/make_fit_optee.sh -t 0x1000 > u-boot.its
./tools/mkimage -f u-boot.its -E -p 0x1400 u-boot.itb -v 0
python3 /port/package.py

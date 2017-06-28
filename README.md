# U-Boot for Ubiquity E200
This repository contains the source for U-Boot for Ubiquity E200 boards (aka
EdgeRouter ER-8 and ERPro-8). It is based on the GPL tarball released by
Ubiquity with a few minor adjustments.

The original tarball name is:

    EdgeRouter ER-8/ERPro-8/EP-R8 Firmware v1.9.11
    GPL.ER-e200.v1.9.1.1.4977353.tar.bz2

The modifications are mostly to get it to build with a "standard" mips toolchain
and to enable a number of missing U-Boot commands.

## Building
Assuming your cross toolchain prefix is `mips-linux-gnu-`, run:

    make CROSS_COMPILE=mips-linux-gnu- octeon_ubnt_e200_config
    make CROSS_COMPILE=mips-linux-gnu- -j$(nproc)

The file `u-boot-octeon_ubnt_e200.bin` should contain the bootloader image. This
image can also be chainloaded from a running U-Boot by simply jumping to the
first instruction.

    Octeon ubnt_e200# tftpboot 0x02000000 u-boot-octeon_ubnt_e200.bin
    Octeon ubnt_e200# go 0x82000000

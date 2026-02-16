# Shared files for the ARM processor #

## algorithm ##

Useful platform independend algorithm by the author of this project.

Files taken from [UniversalboxARM](https://github.com/Solartraveler/UniversalboxArm/tree/a2017e5d17217898de555d77526e7ae967745359/src/common/algorithm) commit #a2017e5d17217898de555d77526e7ae967745359.

License: BSD 3-Clause

## CMSIS ##
CMSIS header files from ARM and ST

License: Apache 2.0

Taken from STM32Cube_FW_F4_V1.28.3

and

STM32Cube_FW_F4_V1.25.2

# fatfs #
Fat filesystem from (http://elm-chan.org/fsw/ff/00index_e.html) version 0.14b.

License: BSD 1-Clause

# jsmn
Json parser from (https://github.com/zserge/jsmn) version 1.1.0

License: MIT

# ledspiellib

Files from the author of this project

Files are simply copied from [UniversalboxARM](https://github.com/Solartraveler/UniversalboxArm/tree/a2017e5d17217898de555d77526e7ae967745359/src/apps/common/boxlib) commit #a2017e5d17217898de555d77526e7ae967745359.
And where needed adapted to the STM32F405 and LedSpiel PCB pin connections.

License: BSD 3-Clause

# libmad
Libmad - MPEG audio decoder library

License: GPL v2 or newer

Taken from GIT [version 0.16.4 commit ee7c3cdd4361b7f7c7da52fa3ba9f76cc35230a9](https://codeberg.org/tenacityteam/libmad/releases/tag/0.16.4)

# lwip
Just for md5sum calculation.

Files taken from [LwIP](https://savannah.nongnu.org/projects/lwip/) version 2.1.3.

License: BSD 3-Clause

## shared-init
Most files are from ST. Linker scripts and init assembly routines.

License: Apache 2.0 and BSD 3-Clause

appInterface.h, stm32f4xx_hal_msp.c, system_stm32f4xx.c, and main.c are taken from
[UniversalboxARM](https://github.com/Solartraveler/UniversalboxArm/blob/a2017e5d17217898de555d77526e7ae967745359/src/apps/common/shared-init/) commit #a2017e5d17217898de555d77526e7ae967745359.

stm32f405-LedSpielV1/main.h is a modified file taken from
[UniversalboxARM](https://github.com/Solartraveler/UniversalboxArm/blob/a2017e5d17217898de555d77526e7ae967745359/src/apps/common/shared-init/stm32f411-nucleo/main.h) commit #a2017e5d17217898de555d77526e7ae967745359.
The modification is mostly CubeMX generated.

stm32f405-LedSpielV1/stm32f4xx_hal_conf.h is taken from
[UniversalboxARM](https://github.com/Solartraveler/UniversalboxArm/blob/a2017e5d17217898de555d77526e7ae967745359/src/apps/common/shared-init/stm32f411-nucleo/stm32f4xx_hal_conf.h) commit #a2017e5d17217898de555d77526e7ae967745359.

## stm32f4xx_HAL_Driver ##
Files from ST.

License: BSD 3-Clause

Taken from STM32Cube_FW_F4_V1.28.3

## libusb_stm32 ##
Usb communication library

License: Apache 2.0

Most files (but one) are taken from
[Libusb_stm32](https://github.com/dmitrystu/libusb_stm32) commit #396c2c50ad330b522104d0565525a7350e486fb9

The stm32_compat.h file is based on:
[stm32h](https://github.com/dmitrystu/stm32h/blob/7e854b1938c6ba5b4163b08de7842e406b5c5fcb/stm32.h) commit #7e854b1938c6ba5b4163b08de7842e406b5c5fcb

License: MIT


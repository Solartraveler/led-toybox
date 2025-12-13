#!/bin/bash

set -e
set -x

LEAVE=":leave"

if [ $# -eq 0 ]; then
  echo "Usage: FILEPREFIX"
  echo "FILEPREFIX: Directory with filenameprefix where a file has the -flash.bin or -ram.tar ending is present"
  exit 2
fi

if [ $# -eq 1 ]; then
  PREFIX=$1
fi

if [ $# -gt 1 ]; then
  echo "Error, too many parameters"
  exit 2
fi

DEVICES=$(dfu-util -l | tee /dev/tty)

if [[ "$DEVICES" == *"name=\"@Internal RAM"* ]]; then
#Use the DFU download firmware provided by the 02-loader project

dfu-util -S "LedLoader" -d 0x1209:7702 -a 0 -s 0x01000$LEAVE -D "$PREFIX-ram.tar"

elif [[ "$DEVICES" == *"name=\"@Internal Flash"* ]]; then
#Use the DFU download program stored in the ROM by ST

dfu-util -a 0 -s 0x08000000$LEAVE -D "$PREFIX-flash.bin"

else

echo "Error, no compatible DFU device detected"

exit 1

fi


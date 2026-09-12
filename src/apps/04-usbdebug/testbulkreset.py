#!/usr/bin/env python3

#Written by Chat-GPT

import sys
import time

import usb.core
import usb.util


# Change these to match your device.
VID = 0x1209
PID = 0x0005

# The firmware handles:
#   bmRequestType = 0x21
#   bRequest      = 0xff
#   wValue        = 0
#   wIndex        = 0
#
# This is a vendor/class-specific OUT request.
REQUEST_TYPE = 0x21
REQUEST = 0xFF
VALUE = 0
INDEX = 0


def main():
    dev = usb.core.find(idVendor=VID, idProduct=PID)

    if dev is None:
        print(f"Device {VID:04x}:{PID:04x} not found", file=sys.stderr)
        return 1

    print(f"Found device {VID:04x}:{PID:04x}")

    # Detach a kernel driver if necessary.
    if dev.is_kernel_driver_active(0):
        try:
            dev.detach_kernel_driver(0)
            print("Detached kernel driver")
        except usb.core.USBError as e:
            print(f"Could not detach kernel driver: {e}", file=sys.stderr)
            return 1

    try:
        dev.set_configuration()

        print("Sending bulk reset...")
        result = dev.ctrl_transfer(
            REQUEST_TYPE,
            REQUEST,
            VALUE,
            INDEX,
            None,       # No data stage
        )

        print(f"Bulk reset completed, returned {result} bytes")
        return 0

    except usb.core.USBError as e:
        print(f"USB error: {e}", file=sys.stderr)
        return 1

    finally:
        usb.util.dispose_resources(dev)


if __name__ == "__main__":
    sys.exit(main())

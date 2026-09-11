=== The original USB implementation has troubles with bulk transfers.
This program is here to help fixing the lib ===

== Key findings of the original lib ==

= Not leaving the loop in usbd_stm32f429_otgfs.c evt_poll =

If USB with ISR is used, the callback for incoming packets must read them
from the hardware FIFO, otherwise the loop in evt_poll is never left.
But reading without loosig data is impossible if these can not be processed
as fast as USB can transfer them and a NAK on the endpoint is needed.
So the callback must notify this to the loop to terminate in such a case.
But then two new issues arise:

1. The lvl status may not generate an ISR anylonger, otherwise the ISR is
immediately re-enterd. And the main loop can not process the data at all.

2. If USBD_SOF_DISABLED is not set, every SOF ISR again checks the RXFLVL signal
and the loop skips the part clearing the SOF ISR and as result again the ISR
is immediately re-enterd.

= Configuring an endpoint may only be done if it was deconfigured before =

If usbd_ep_config for an endpoint was called, it may not be called a second
time, because every call allocates another 64 bytes of the 1280 maximum buffer.
As result having 2 endpoints (64 bytes each), this fails at the 20th call.

And every opening by a user application does a set config.

== Testing this project ==

Give the userspace access to the device by copying 55-testdevice5.rules to /etc/udev/rules.d/

Then

udevadm control --reload-rules

udevadm trigger --subsystem-match=usb

Call ./testbulk.py 178 64 to see if the USB bulk endpoint is working as expected.
Without the issues above, sending more than USB_BULK_QUEUE_FROMHOST_LEN = 50
packets might result in errors.
After fixing the issues, sending more than (USB_BULK_QUEUE_FROMHOST_LEN + USB_BULK_QUEUE_TOHOST_LEN) = 178 packets might result in errors.
Actually most of the time ~5 more packets will work due to some processing or host side queueing.

== Debugging ==
1. Connect a STM32 Nucleo to the target

2. Connect openocd

cp /usr/share/openocd/scripts/target/stm32f4x.cfg stm32f4x-slow.cfg

Replace all values behing 'adapter speed' by 400 (three times in the file).

openocd -f /usr/share/openocd/scripts/interface/stlink.cfg -f stm32f4x-slow.cfg

3. In another console connect with gdb

Use the ram.elf if uploaded temporary by my DFU bootloader, use the flash.elf if the ST bootloader has been used.

gdb-multiarch build/stm32f405xx-LedSpielV1/UsbDebug-stm32f405xx-ram.elf -ex "target remote localhost:3333"

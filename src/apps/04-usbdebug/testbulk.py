#!/usr/bin/env python3

#Written by Chat-GPT

import sys
import random
import usb.core
import usb.util


# ---------------------------------------------------------------------------
# USB device configuration
# ---------------------------------------------------------------------------

VID = 0x1209
PID = 0x0005

INTERFACE = 0

BULK_IN_EP = 0x81  # Device -> host, endpoint 1
BULK_OUT_EP = 0x02 # Host -> device, endpoint 2

TIMEOUT_MS = 2000


def find_device():
    """Find and open the USB device."""

    dev = usb.core.find(idVendor=VID, idProduct=PID)

    if dev is None:
        raise RuntimeError(
            f"USB device {VID:04x}:{PID:04x} not found"
        )

    # Select the first configuration.
    dev.set_configuration()

    # Get the active configuration/interface.
    cfg = dev.get_active_configuration()
    intf = cfg[(INTERFACE, 0)]

    # Detach a kernel driver if one happens to be attached.
    if dev.is_kernel_driver_active(INTERFACE):
        print("Detaching kernel driver...")
        dev.detach_kernel_driver(INTERFACE)

    usb.util.claim_interface(dev, INTERFACE)

    return dev


def make_packet(length):
    """Generate a pseudo-random packet."""

    return bytes(random.getrandbits(8) for _ in range(length))

def describe_mismatch(expected, received):
    """Print details about a packet mismatch."""

    print(f"  Expected length: {len(expected)}")
    print(f"  Received length: {len(received)}")

    min_len = min(len(expected), len(received))

    first_diff = None

    for i in range(min_len):
        if expected[i] != received[i]:
            first_diff = i
            break

    if first_diff is not None:
        print(
            f"  First difference at byte {first_diff}: "
            f"expected 0x{expected[first_diff]:02x}, "
            f"received 0x{received[first_diff]:02x}"
        )

    print(f"  Expected: {expected.hex()}")
    print(f"  Received: {received.hex()}")

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <number_of_packets> <packet_length>")
        print()
        print("Example:")
        print(f"  {sys.argv[0]} 100 64")
        sys.exit(1)

    try:
        num_packets = int(sys.argv[1])
        packet_length = int(sys.argv[2])
    except ValueError:
        print("Error: number_of_packets and packet_length must be integers.")
        sys.exit(1)

    if num_packets <= 0:
        print("Error: number_of_packets must be > 0")
        sys.exit(1)

    if packet_length <= 0:
        print("Error: packet_length must be > 0")
        sys.exit(1)

    print(f"Packets       : {num_packets}")
    print(f"Packet length : {packet_length}")
    print(f"USB device    : {VID:04x}:{PID:04x}")
    print(f"Bulk OUT      : 0x{BULK_OUT_EP:02x}")
    print(f"Bulk IN       : 0x{BULK_IN_EP:02x}")
    print()

    dev = None

    try:
        dev = find_device()

        print("USB device opened.")
        print("Sending packets...")

        # Keep the packets so that we can compare the received data.
        sent_packets = []

        # ------------------------------------------------------------------
        # Send packets
        # ------------------------------------------------------------------

        for packet_number in range(num_packets):
            packet = make_packet(packet_length)
            sent_packets.append(packet)

            transferred = dev.write(
                BULK_OUT_EP,
                packet,
                timeout=TIMEOUT_MS
            )

            if transferred != packet_length:
                print(
                    f"ERROR: packet {packet_number}: "
                    f"only sent {transferred}/{packet_length} bytes"
                )
                return 1

            if (packet_number + 1) % 100 == 0 or packet_number == num_packets - 1:
                print(
                    f"\rSent {packet_number + 1}/{num_packets}",
                    end="",
                    flush=True
                )

        print()
        print("All packets sent.")
        print("Receiving packets...")

        # ------------------------------------------------------------------
        # Receive and compare packets
        # ------------------------------------------------------------------

        errors = 0

        # Keep track of packets which have already been matched.
        received_packet_numbers = set()

        for packet_number in range(num_packets):

            try:
                received = bytes(
                    dev.read(
                        BULK_IN_EP,
                        packet_length,
                        timeout=TIMEOUT_MS
                    )
                )

            except usb.core.USBTimeoutError:
                print(
                    f"\nERROR: timeout waiting for packet "
                    f"{packet_number}"
                )
                errors += 1
                continue

            expected = sent_packets[packet_number]

            # --------------------------------------------------------------
            # Normal case
            # --------------------------------------------------------------

            if received == expected:
                received_packet_numbers.add(packet_number)

                if (
                    (packet_number + 1) % 100 == 0
                    or packet_number == num_packets - 1
                ):
                    print(
                        f"\rReceived {packet_number + 1}/{num_packets}",
                        end="",
                        flush=True
                    )

                continue

            # --------------------------------------------------------------
            # Mismatch
            # --------------------------------------------------------------

            errors += 1

            print()
            print("=" * 70)
            print(f"ERROR: packet {packet_number} does not match!")

            describe_mismatch(expected, received)

            # --------------------------------------------------------------
            # Search all packets for the received data.
            #
            # This tells us whether the device returned:
            #
            #   - an old packet
            #   - a future packet
            #   - some completely unexpected data
            # --------------------------------------------------------------

            matching_packets = [
                i for i, packet in enumerate(sent_packets)
                if packet == received
            ]

            if matching_packets:

                print(
                    f"  Received data matches sent packet(s): "
                    f"{matching_packets}"
                )

                old_packets = [
                    i for i in matching_packets
                    if i < packet_number
                ]

                future_packets = [
                    i for i in matching_packets
                    if i > packet_number
                ]

                if old_packets:
                    print(
                        f"  WARNING: old packet received: "
                        f"packet {old_packets}"
                    )

                if future_packets:
                    print(
                        f"  WARNING: future packet received: "
                        f"packet {future_packets}"
                    )

                    # Determine which packets were skipped.
                    first_future = future_packets[0]

                    if first_future > packet_number + 1:
                        skipped = list(
                            range(packet_number, first_future)
                        )

                        print(
                            f"  WARNING: device appears to have skipped "
                            f"packet(s): {skipped}"
                        )

            else:
                print(
                    "  Received data does not match ANY sent packet."
                )

            print("=" * 70)

        print()
        print("=" * 70)

        # ------------------------------------------------------------------
        # Final analysis
        # ------------------------------------------------------------------

        missing_packets = [
            i for i in range(num_packets)
            if i not in received_packet_numbers
        ]

        if missing_packets:
            print(
                f"Packets not received in the expected position: "
                f"{missing_packets}"
            )

        if errors == 0:
            print(
                f"PASS: all {num_packets} packets matched correctly."
            )
            print("=" * 70)
            return 0

        print(
            f"FAIL: {errors}/{num_packets} packet(s) had errors."
        )

        print("=" * 70)

        return 1

    except usb.core.USBError as e:
        print(f"\nUSB error: {e}")
        return 1

    except RuntimeError as e:
        print(f"\nError: {e}")
        return 1

    finally:
        if dev is not None:
            try:
                usb.util.release_interface(dev, INTERFACE)
            except usb.core.USBError:
                pass

            usb.util.dispose_resources(dev)


if __name__ == "__main__":
    sys.exit(main())


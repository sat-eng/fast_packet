#!/usr/bin/env python3
"""UDP multicast sender compatible with FastPacket's PacketReceiver, for
testing the receiver from a Linux (or any other) machine without running
the Qt sender. First 4 bytes of each packet = little-endian uint32
sequence number, matching what PacketReceiver.cpp reads."""
import argparse
import random
import socket
import struct
import time

def main():
    parser = argparse.ArgumentParser(description="FastPacket-compatible UDP multicast sender")
    parser.add_argument("--group", default="225.10.100.100", help="multicast group address")
    parser.add_argument("--port", type=int, default=105, help="destination port")
    parser.add_argument("--pkt-size", type=int, default=1427, help="packet size in bytes")
    parser.add_argument("--rate-mbps", type=float, default=2.0, help="target data rate in Mbit/s")
    parser.add_argument("--ttl", type=int, default=32, help="multicast TTL")
    parser.add_argument("--loss-rate", type=float, default=0.0,
                         help="probability (0.0-1.0) of dropping each packet, to simulate loss")
    args = parser.parse_args()

    if not 0.0 <= args.loss_rate <= 1.0:
        parser.error("--loss-rate must be between 0.0 and 1.0")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, args.ttl)

    payload = b'\x00' * (args.pkt_size - 4)
    delay = (args.pkt_size * 8) / (args.rate_mbps * 1_000_000)

    seq = 0
    sent = 0
    dropped = 0
    try:
        while True:
            # Sequence number always increments, even when a packet is dropped,
            # so the receiver's gap-detection sees the simulated loss.
            if args.loss_rate == 0.0 or random.random() >= args.loss_rate:
                pkt = struct.pack('<I', seq) + payload
                sock.sendto(pkt, (args.group, args.port))
                sent += 1
            else:
                dropped += 1

            seq += 1
            time.sleep(delay)

            if seq % 500 == 0:
                print(f"seq={seq} sent={sent} dropped={dropped} "
                      f"({100.0 * dropped / seq:.2f}% loss so far)")
    except KeyboardInterrupt:
        print(f"\nStopped. seq={seq} sent={sent} dropped={dropped}")

if __name__ == "__main__":
    main()

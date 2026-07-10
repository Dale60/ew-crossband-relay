#!/usr/bin/env python3
"""UDP collector for EW cross-band bridge batches.

Listens for "EWB1" batches from the bridge firmware and prints each
contained frame with timestamp, hex and ASCII. Zero dependencies.

Usage: python udp_collector.py [port]   (default 9000)
"""

import socket
import sys
import time


def printable(b: bytes) -> str:
    return "".join(chr(c) if 32 <= c < 127 else "." for c in b)


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9000
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", port))
    print("listening on udp/%d" % port)

    while True:
        data, addr = s.recvfrom(2048)
        ts = time.strftime("%H:%M:%S")
        if len(data) < 5 or data[:4] != b"EWB1":
            print("%s %s: %d bytes (not EWB1): %s"
                  % (ts, addr[0], len(data), data[:32].hex()))
            continue
        count = data[4]
        off = 5
        print("%s %s: batch of %d frame(s), %d bytes"
              % (ts, addr[0], count, len(data)))
        for i in range(count):
            if off >= len(data):
                print("  frame %d: truncated batch" % (i + 1))
                break
            ln = data[off]
            off += 1
            payload = data[off:off + ln]
            off += ln
            print("  frame %d (%d B): %s  | %s"
                  % (i + 1, ln, payload.hex(), printable(payload)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

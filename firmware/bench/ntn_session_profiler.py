#!/usr/bin/env python3
"""NTN session energy profiler harness for nRF9151 SMA DK + SLM.

Drives the serial_lte_modem application over UART through repeated
NTN attach / uplink / teardown cycles and timestamps every transition
so the log can be aligned with a PPK2 current capture.

Alignment: before each session the script pulses RTS for 50 ms; wire
RTS to a PPK2 digital input (or scope channel) as the sync marker.

Usage:
  python ntn_session_profiler.py --port COM7 --sessions 30 \
      --payload-bytes 200 --out energy_sessions.csv

ASCII only. Requires: pyserial (pip install pyserial).
"""

import argparse
import csv
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial not installed: pip install pyserial")

# ---------------------------------------------------------------------------
# AT command set. VERIFY against nRF91x1 NTN AT Commands Reference Guide v1.2
# for the modem FW actually installed. Placeholders marked TODO must be
# confirmed before a measurement run is treated as valid.
# ---------------------------------------------------------------------------
AT = {
    "echo_off":        "ATE0",
    "fw_version":      "AT+CGMR",
    "sysmode_ntn":     "AT%XSYSTEMMODE=0,1,0,0",   # TODO: NTN NB-IoT flags per v1.2 guide
    "apn":             None,                        # TODO: operator APN via AT+CGDCONT if required
    "cfun_on":         "AT+CFUN=1",
    "cfun_off":        "AT+CFUN=0",
    "reg_query":       "AT+CEREG?",
    "reg_urc_on":      "AT+CEREG=5",
    "coneval":         "AT%CONEVAL",
    # SLM UDP socket commands (confirm against SLM docs for installed NCS):
    "udp_open":        "AT#XSOCKET=1,2,0",
    "udp_connect":     None,  # set at runtime from --host/--udp-port
    "udp_send_prefix": "AT#XSEND=",
    "udp_close":       "AT#XSOCKET=0",
}

REGISTERED_STATS = {"1", "5"}  # home, roaming


def now() -> float:
    return time.monotonic()


class Dut:
    def __init__(self, port: str, baud: int, log):
        self.s = serial.Serial(port, baud, timeout=0.2)
        self.log = log

    def pulse_marker(self, ms: int = 50) -> None:
        self.s.rts = True
        time.sleep(ms / 1000.0)
        self.s.rts = False

    def cmd(self, at: str, ok=("OK",), err=("ERROR",), timeout=10.0) -> tuple:
        """Send one AT command, collect lines until OK/ERROR/timeout."""
        self.s.reset_input_buffer()
        self.s.write((at + "\r\n").encode("ascii"))
        t0 = now()
        lines = []
        while now() - t0 < timeout:
            raw = self.s.readline()
            if not raw:
                continue
            line = raw.decode("ascii", errors="replace").strip()
            if not line:
                continue
            lines.append(line)
            if any(line.startswith(k) for k in ok):
                return True, lines, now() - t0
            if any(line.startswith(k) for k in err):
                return False, lines, now() - t0
        return False, lines, timeout

    def wait_registered(self, timeout: float) -> tuple:
        """Poll +CEREG until registered or timeout. Returns (ok, seconds)."""
        t0 = now()
        while now() - t0 < timeout:
            ok, lines, _ = self.cmd(AT["reg_query"], timeout=5.0)
            for line in lines:
                if "+CEREG" in line:
                    parts = line.split(",")
                    if len(parts) >= 2 and parts[1].strip() in REGISTERED_STATS:
                        return True, now() - t0
            time.sleep(2.0)
        return False, now() - t0


def run(args) -> int:
    fieldnames = [
        "session", "t_marker", "t_attach_s", "attach_ok",
        "t_send_s", "send_ok", "payload_bytes", "t_off_s", "coneval",
    ]
    out = csv.DictWriter(open(args.out, "w", newline=""), fieldnames)
    out.writeheader()

    dut = Dut(args.port, args.baud, print)
    dut.cmd(AT["echo_off"])
    print(dut.cmd(AT["fw_version"])[1])
    if AT["sysmode_ntn"]:
        dut.cmd(AT["cfun_off"])
        dut.cmd(AT["sysmode_ntn"])

    payload = "A" * args.payload_bytes

    for n in range(1, args.sessions + 1):
        row = {"session": n, "payload_bytes": args.payload_bytes}
        dut.pulse_marker()
        row["t_marker"] = time.time()

        t0 = now()
        dut.cmd(AT["cfun_on"], timeout=15.0)
        ok, t_attach = dut.wait_registered(args.attach_timeout)
        row["t_attach_s"] = round(t_attach, 2)
        row["attach_ok"] = int(ok)

        row["t_send_s"] = ""
        row["send_ok"] = 0
        if ok:
            dut.cmd(AT["udp_open"], timeout=10.0)
            dut.cmd("AT#XCONNECT=\"%s\",%d" % (args.host, args.udp_port),
                    timeout=10.0)
            t1 = now()
            sok, _, _ = dut.cmd(
                AT["udp_send_prefix"] + "\"" + payload + "\"",
                timeout=args.send_timeout)
            row["t_send_s"] = round(now() - t1, 2)
            row["send_ok"] = int(sok)
            _, cv, _ = dut.cmd(AT["coneval"], timeout=5.0)
            row["coneval"] = ";".join(cv)[:120]
            dut.cmd(AT["udp_close"], timeout=5.0)
        else:
            row["coneval"] = ""

        dut.cmd(AT["cfun_off"], timeout=15.0)
        row["t_off_s"] = round(now() - t0, 2)
        out.writerow(row)
        print("session %d: attach=%s %.1fs send=%s total=%.1fs"
              % (n, row["attach_ok"], t_attach, row["send_ok"], row["t_off_s"]))
        time.sleep(args.gap_s)

    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--sessions", type=int, default=30)
    p.add_argument("--payload-bytes", type=int, default=200)
    p.add_argument("--host", default="192.0.2.1",
                   help="UDP echo endpoint for uplink test")
    p.add_argument("--udp-port", type=int, default=9000)
    p.add_argument("--attach-timeout", type=float, default=300.0)
    p.add_argument("--send-timeout", type=float, default=120.0)
    p.add_argument("--gap-s", type=float, default=30.0)
    p.add_argument("--out", default="energy_sessions.csv")
    return run(p.parse_args())


if __name__ == "__main__":
    sys.exit(main())

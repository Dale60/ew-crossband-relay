# Bench mule firmware plan - gates VERIFY 6 and VERIFY 9

One rig answers both gates: nRF9151 SMA DK + SX1262 shield + PPK2 + logic
analyser + Skylo-capable NTN SIM.

## Gate 1 (VERIFY 9): energy per Skylo GEO session

Method: PPK2 in source-meter mode powering the DK at 3.6 V (repeat at
3.0 V for end-of-life). Modem FW: current NTN release (aligned with NTN AT
guide v1.2 - NOT v1.0.0, see amendment A2). Application: Nordic
serial_lte_modem (SLM), driven by ntn_session_profiler.py over UART.

Profile per session, 30 sessions minimum across a day:
- cold attach energy (power-on to registered, NTN sysmode)
- uplink energy for 50 / 200 / 500 byte payloads (UDP and, if operator
  supports it, NIDD - closes VERIFY 1 as a side effect)
- teardown vs PSM-retention comparison (does keeping PSM context beat
  full power-off given the fixed attach cost - decides duty-cycle policy)
- GNSS fix contribution: static-position cache vs fresh fix per session

Output: energy_sessions.csv -> replaces model assumption A3; re-run the
Section 5 model and fix the served-device ceiling.

## Gate 2 (VERIFY 6): LoRa RX-window jitter under NTN activity

Method: LoRa Basics Modem (TS011 relay build) running on the 9151 app core
driving the SX1262, while the modem core runs NTN attach/TX cycles.
Instrument with GPIO toggles at: CAD start, WOR detect, relay RX-window
open/close, NTN modem event callbacks. Capture on logic analyser.

Pass criterion (proposed, adjust after first capture): relay RX-window
open jitter < 1 ms p99.9 across 10k windows with NTN worst-case activity
(attach + max-length TX) forced concurrent. Fail -> Variant A needs a
scheduling redesign (Zephyr priority/ISR rework) or a co-processor, and
the verdict changes.

## Files

- ntn_session_profiler.py - UART/AT harness, timestamps every state
  transition, emits CSV aligned to PPK2 capture via a marker pulse.
  AT strings for NTN sysmode are placeholders at the top of the file;
  fill from the nRF91x1 NTN AT Commands Reference Guide v1.2 for the
  installed FW before running.
- (to come) lbm_jitter/ - LBM relay build with GPIO instrumentation patch.

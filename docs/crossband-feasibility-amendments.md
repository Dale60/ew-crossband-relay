# Feasibility Pack - Amendments (2026-07-10, post-session review)

Appended after independent review of the cloud-session pack. The original
document is left untouched for audit trail. These amendments close or
redirect four items.

## A1. VERIFY item 8 - CLOSED (nRF9151 TX current, first-hand)

Source: Nordic nRF9151 Hardware Design Guidelines (nwp_056), HTML pages
(no PDF required):
- IoT NTN TX, PC3 23 dBm (bands B255/B256/B252/B23), average current during
  TX subframe, 50 ohm typical: 285 mA at 3.7 V / 25 C; 340-345 mA at
  3.0 V / 25 C; 375 mA at 3.0 V / 85 C. Into VSWR 3:1 worst rows reach
  ~470 mA. Modulation peaks exceed these averages for microsecond-scale
  bursts; Nordic notes added VDD capacitance flattens the peaks.
- Terrestrial NB-IoT PC3 sits in the same envelope (roughly 260-390 mA
  typ. across band/voltage/temperature).
- NTN supports Power Class 3 only. PC5 (20 dBm) applies to terrestrial
  fallback only and is irrelevant to the NTN power budget.

Design consequence: size the Li-SOCl2 + HLC rail for ~400 mA sustained,
~500 mA peak capability at end-of-life cell voltage. Assumption A3
(0.6 mAh/session) is now a defensible midpoint of the 0.3-1.0 mAh band;
bench measurement still required but the model centre is anchored.
For AU deployments use the 55 C / 85 C table rows, not -40 C.

## A2. VERIFY item 4 - PARTIALLY CLOSED (modem FW currency)

A maintenance NTN modem FW newer than v1.0.0 exists (aligned with v1.2 of
the nRF91x1 NTN AT Commands Reference Guide). Its NTN fixes include:
- premature attach-context expiry, and
- dropped NB-IoT uplink messages on power-off after transmission.
The second bug is directly our duty-cycle pattern (modem powered down after
each backhaul session). Baseline all prototype work on the current FW, not
the December 2025 v1.0.0. Remaining open: NIDD vs IP/UDP per operator, and
NTN PSM/eDRX floor currents (items 1-3 unchanged).

## A3. VERIFY item 14 - REFRAMED (Iridium NTN Direct)

Pack said Rel-19 support on Rel-17 silicon is "expected no". Nordic's own
material states Iridium NTN Direct will be incorporated into the SiP as
part of the 3GPP Release 19 NTN roadmap. The open question is whether that
lands as a firmware update on current LACA silicon or requires new silicon.
Reframe: "confirm Iridium Rel-19 support path for current nRF9151 LACA",
expected direction positive-via-firmware, timing unknown.

## A4. ADDITION to executive verdict - AU market window

ProEsys backhauls via the EchoStar Mobile EM2050 (S-band LoRaWAN). EchoStar
Mobile's S-band MSS footprint is European. Unless the pictured C-band
variant has an AU-capable operator behind it, ProEsys most likely cannot
serve Australia today, while Skylo GEO over AU is already certified on the
nRF9151. This is a home-market window and belongs in the build decision.
VERIFY (new item 16): confirm EchoStar Mobile / C-band coverage over AU.

Minor gap noted: brief task 7 asked for the Kerlink iStation M2 satellite
roadmap (Kineis); the pack covered only the terrestrial iStation.

Net effect: both CONDITIONAL BUILD verdicts stand. VERIFY list is now 12
open items. The gating spend is unchanged and unambiguous: one bench mule
answering items 6 and 9 simultaneously.

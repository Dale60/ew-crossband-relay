# Hardware - EW Cross-Band Relay (Variant A first)

Strategy: two stages. Do not spin a PCB until Stage 1 gates pass.

## Stage 1 - bench mule (no custom PCB)

Purpose: close VERIFY 6 (single-M33 LoRa/NTN coexistence) and VERIFY 9
(measured energy per Skylo GEO session).

Shopping list:
- nRF9151 SMA DK (SMA variant, for PPK2 current measurement and external
  antennas; ships with Taoglas LTE/NTN and Kyocera GNSS antennas and
  NTN-capable trial SIMs)
- SX1262 mbed shield (SX1262MB2CAS, 915 MHz variant) on the DK Arduino
  headers, or an SX1262 breakout wired to SPI + BUSY + DIO1 + NSS + NRESET
- Power Profiler Kit II (PPK2) in source-meter mode
- Logic analyser (window-jitter capture)
- NTN SIM with Skylo AU access (Monogoto or equivalent; confirm AU
  roaming/coverage on order)

## Stage 2 - prototype PCB, donor-based schematic assembly

Do not draw from scratch. Assemble from four donors:

1. Optima (in-house). The nRF9151 core, SIM/iSIM, PSU sequencing and RF
   front end are already proven in our own product. This is the primary
   donor for the 9151 section - reuse verbatim where possible.
2. Nordic nRF9151 DK hardware files. Published by Nordic in native Altium
   format - direct copy-paste donor inside Altium for anything Optima does
   not cover (e.g. SMA/test structures, MAGPIO usage).
3. Circuit Dojo nRF9151 Feather (open source, KiCad). Compact 9151
   implementation with nPM1300; useful cross-check, importable to Altium
   via the KiCad importer.
4. Semtech SX1262 reference (SX1262MB2xAS / DVK, AN1200.xx matching).
   Donor for the LoRa section: 32 MHz TCXO (DIO3-powered option), DIO2
   RX/TX switch control, 915 MHz matching and harmonic filter.

### Delta blocks (the only genuinely new schematic content)

D1. Energy front end: 2x Li-SOCl2 D cells (Saft LS33600 / Tadiran SL-2780
    class) in parallel with a hybrid-layer capacitor (Tadiran HLC-1550A
    class) behind a passivation-managed series element. Rail must deliver
    ~400 mA sustained / ~500 mA peak at end-of-life voltage per amendment
    A1. Final HLC sizing from Stage 1 measured burst profile.

D2. SX1262 radio section per donor 4, SPI-mastered by the nRF9151 app
    core. GPIO budget: SPI(4) + BUSY + DIO1 + NRESET + optional DIO2
    switch sense = 8 pins; trivially inside the 9151's 32 GPIO.

D3. Antenna plan (two antennas, not three): the nRF9151 exposes a single
    50 ohm ANT pin covering 700-2200 MHz for LTE, NTN L-band (B255/B256)
    and GNSS (time-multiplexed) - use a wideband LTE/NTN element (Taoglas
    class, as on the SMA DK). Second antenna is the 915 MHz LoRa omni.
    Desense budget: 915 MHz TX second harmonic (~1830 MHz) and broadband
    TX noise into the 9151 ANT path; mitigate with PA-output harmonic
    filter, physical separation, and firmware time-division (never LoRa-TX
    during an NTN RX or GNSS window). Quantify isolation in Stage 1.

## Altium pipeline (what actually works)

- Nordic DK donor is native Altium: open alongside the project and copy
  sections directly.
- Anything authored here arrives as KiCad source or netlist: use Altium's
  File > Import Wizard (KiCad) - schematic and layout both import.
- Altium 365 web viewer: share the workspace link and design reviews /
  mark-ups can be done against it via browser; desktop-only operations
  (cut-and-paste between projects) remain manual on your side.
- Fastest concrete next step: export the Optima schematic (PDF + netlist)
  into this repo under hardware/donors/ and the D1-D3 delta wiring will be
  produced against real net names.

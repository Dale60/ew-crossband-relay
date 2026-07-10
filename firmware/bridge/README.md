# bridge - UART-to-NTN backhaul glue (bench hack)

Glues any LoRa board with an MCU to an nRF9151 DK over UART, batching
framed uplinks into UDP over NB-IoT - terrestrial or NB-NTN with one
#define. Data can flow this afternoon on a terrestrial SIM; flip to NTN
when the Skylo-capable SIM arrives. Same modem firmware covers both.

## Two hookup options

A) UART glue (this app, works today): LoRa board keeps its own firmware,
   emits received LoRa payloads as framed bytes on a UART pin. The 9151
   queues and backhauls them. Proves the whole pipe end to end and closes
   VERIFY 9 (session energy) with the PPK2. Does NOT test VERIFY 6
   (single-core LoRa/NTN coexistence) - both radios are not on one M33.

B) SX1262 on the 9151's SPI (next deliverable): the real Variant A rig.
   Needs a bare SX1262 (shield/breakout, no MCU) wired to the DK header.
   That build adds the LoRa driver to this app and answers VERIFY 6.

## Prerequisites

- Flash the LATEST NTN modem firmware (mfw_nrf9151-ntn, the maintenance
  release aligned with NTN AT guide v1.2, not v1.0.0). It also supports
  terrestrial LTE-M/NB-IoT, so one flash covers both modes.
- NCS v3.2.0 or later toolchain.
- Board target: nRF9151 DK is nrf9151dk/nrf9151/ns. For the SMA DK check
  `west boards | grep 9151` in your NCS version and substitute.

## Build and flash

    west build -p -b nrf9151dk/nrf9151/ns firmware/bridge
    west flash

## Wiring (option A)

    LoRa board TX  ->  DK uart1 RX (see board DTS for the header pin)
    GND            ->  GND
    3.3 V logic both sides. 115200 8N1.

uart1 is enabled by the overlay in this directory; confirm which header
pins the board DTS routes uart1 to on your DK revision before wiring.

## Frame format (LoRa board -> 9151)

    [0xA5][len][payload bytes][crc8]

len = 1..96. crc8 poly 0x07 (MSB first, init 0x00) over len + payload.
Anything malformed is dropped and counted.

PC injector for testing without any LoRa board attached (USB-UART on the
same pins):

    python -c "import serial,sys; p=serial.Serial(sys.argv[1],115200);\
    d=b'hello-ew'; c=0\n" # see tools snippet in repo wiki, or use
    ntn_session_profiler.py as a template

Simplest: send bytes A5 08 68 65 6C 6C 6F 2D 65 77 CRC from any serial
tool; the app logs accepted frames and queue depth.

## Configuration (top of src/main.c)

- NTN_ENABLE       0 = terrestrial NB-IoT (bring-up), 1 = NB-NTN
- NTN_BANDLOCK     "255" for Skylo L-band GSO testing, "" to disable
- STATIC_LOCATION  1 = inject fixed lat/lon (static relay, skips GNSS)
- UDP_HOST/PORT    your collector endpoint
- BACKHAUL_PERIOD_S / QUEUE_FLUSH_AT   duty cycle policy
- DUTY_POWER_OFF   1 = CFUN=0 between sessions (mirrors relay duty and
                   the energy test), 0 = stay attached (PSM comparison)

## Bring-up order

1. NTN_ENABLE=0, terrestrial SIM, confirm frames arrive at collector.
2. PPK2 on, 30 sessions, capture terrestrial baseline energy.
3. NTN_ENABLE=1 + Skylo SIM + clear sky, repeat. Delta = the model's A3.

## Verify notes

- %LOCATION parameter semantics taken from a Nordic DevZone-documented
  sequence; confirm against the nRF91x1 NTN AT Commands Reference Guide
  v1.2 before trusting positions other than the default test values.
- lte_lc is configured to leave system mode alone
  (CONFIG_LTE_NETWORK_MODE_DEFAULT); this app owns %XSYSTEMMODE.

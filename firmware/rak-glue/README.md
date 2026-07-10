# rak-glue - RAK4631 LoRa-to-UART feeder

Turns a RAK4631 WisBlock (nRF52840 + SX1262) into the LoRa front end of
the bench mule: continuous LoRa RX, every received frame re-emitted on
UART in the bridge frame format (0xA5 / len / payload / crc8).

Check the module marking first: RAK4631 (Arduino BSP) uses this sketch
as-is. RAK4631-R runs RUI3 and needs a port - say so and it will be
written.

## Toolchain

- Arduino IDE / arduino-cli with the RAKwireless nRF BSP installed
  (RAK4631 board target)
- Library: SX126x-Arduino (library manager)

## Wiring (3 wires)

    WisBlock base TXD1  ->  nRF9151 DK uart1 RX
    WisBlock base RXD1  ->  (unused, downlink later)
    GND                 ->  GND

Both sides 3.3 V logic, 115200 8N1. Confirm which header pins the 9151
DK board DTS routes uart1 to before wiring (see bridge README).

## Bring-up order

1. HEARTBEAT_S is 30 by default: the sketch emits a synthetic frame
   every 30 s with no LoRa traffic at all. Flash, wire, and confirm the
   bridge logs frames and the collector prints them - the whole pipe is
   proven before any radio settings matter.
2. Then set RF_FREQUENCY / LORA_SF / PUBLIC_SYNCWORD to match a test
   node locked to a single AU915 channel, and real LoRa frames flow.

## Notes

- Sniffing a LoRaWAN node this way captures raw PHY frames (MAC header
  in clear, payload encrypted). For the bench mule that is exactly what
  a relay forwards anyway; NS-side handling is out of scope here.
- Keep the node on a fixed channel/SF for the bench - no hopping.
- If RX callbacks never fire on your library version, add
  Radio.IrqProcess() inside loop() (needed on some lib variants).

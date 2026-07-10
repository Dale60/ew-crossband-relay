/*
 * RAK4631 (nRF52840 + SX1262) LoRa-to-UART glue for the EW cross-band
 * bench mule. Continuous LoRa RX; each frame re-emitted on Serial1 as
 * [0xA5][len][payload][crc8 poly 0x07 over len+payload], 115200 8N1.
 *
 * HEARTBEAT_S > 0 also emits a synthetic frame periodically so the
 * UART/bridge/backhaul pipe can be proven with no LoRa traffic at all.
 *
 * Library: SX126x-Arduino. Board: RAK4631 (Arduino BSP, not RUI3).
 */

#include <Arduino.h>
#include <SX126x-RAK4630.h>

/* ---------------- configuration ---------------- */
#define RF_FREQUENCY      916800000  /* Hz - AU915 bench channel, match node */
#define LORA_BANDWIDTH    0          /* 0 = 125 kHz */
#define LORA_SF           7
#define LORA_CODINGRATE   1          /* 1 = 4/5 */
#define LORA_PREAMBLE_LEN 8
#define PUBLIC_SYNCWORD   true       /* true 0x34 (LoRaWAN), false 0x12 (P2P) */
#define HEARTBEAT_S       30         /* 0 disables synthetic frames */
#define GLUE_BAUD         115200
#define MAX_FRAME         96

#define SOF 0xA5

static RadioEvents_t RadioEvents;
static uint32_t rx_count;
static uint32_t hb_count;
static unsigned long hb_last_ms;

static uint8_t crc8(uint8_t crc, const uint8_t *p, size_t n)
{
  while (n--) {
    crc ^= *p++;
    for (int i = 0; i < 8; i++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                         : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

static void emit_frame(const uint8_t *payload, uint8_t len)
{
  uint8_t crc = crc8(0x00, &len, 1);
  crc = crc8(crc, payload, len);
  Serial1.write(SOF);
  Serial1.write(len);
  Serial1.write(payload, len);
  Serial1.write(crc);
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
  uint8_t len = (size > MAX_FRAME) ? MAX_FRAME : (uint8_t)size;

  rx_count++;
  emit_frame(payload, len);
  Serial.printf("rx #%lu len=%u rssi=%d snr=%d\r\n",
                (unsigned long)rx_count, len, rssi, snr);
  digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
  Radio.Rx(0);
}

static void OnRxTimeout(void) { Radio.Rx(0); }
static void OnRxError(void)   { Radio.Rx(0); }

void setup()
{
  pinMode(LED_GREEN, OUTPUT);
  Serial.begin(115200);      /* USB debug */
  Serial1.begin(GLUE_BAUD);  /* glue to nRF9151 DK uart1 */
  delay(500);
  Serial.println("EW rak-glue: LoRa RX -> UART frames");

  lora_rak4630_init();

  RadioEvents.RxDone    = OnRxDone;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError   = OnRxError;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetPublicNetwork(PUBLIC_SYNCWORD);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SF, LORA_CODINGRATE,
                    0, LORA_PREAMBLE_LEN, 0 /* symb timeout */,
                    false /* fixLen */, 0, true /* crcOn */,
                    0, 0, false /* iqInverted */, true /* continuous */);
  Radio.Rx(0);
  Serial.printf("listening %lu Hz SF%d BW125 sync=%s\r\n",
                (unsigned long)RF_FREQUENCY, LORA_SF,
                PUBLIC_SYNCWORD ? "public" : "private");
}

void loop()
{
#if HEARTBEAT_S > 0
  if (millis() - hb_last_ms >= (unsigned long)HEARTBEAT_S * 1000UL) {
    uint8_t buf[24];
    int n = snprintf((char *)buf, sizeof(buf), "EW-HB-%lu",
                     (unsigned long)++hb_count);
    emit_frame(buf, (uint8_t)n);
    Serial.printf("heartbeat #%lu\r\n", (unsigned long)hb_count);
    hb_last_ms = millis();
  }
#endif
  delay(50);
}

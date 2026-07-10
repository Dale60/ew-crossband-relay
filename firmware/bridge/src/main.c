/*
 * EW Cross-Band Relay - bench bridge
 *
 * Framed uplinks in over UART -> RAM queue -> batched UDP out over
 * NB-IoT, terrestrial or NB-NTN (NTN_ENABLE). Duty-cycled modem to
 * mirror the Variant A power profile.
 *
 * Frame in: [0xA5][len 1..96][payload][crc8 poly 0x07 over len+payload]
 * Batch out: "EWB1" [count u8] then per frame [len u8][payload]
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <nrf_modem_at.h>
#include <string.h>

LOG_MODULE_REGISTER(bridge, LOG_LEVEL_INF);

/* ---------------- configuration ---------------- */
#define NTN_ENABLE        0            /* 0 terrestrial NB-IoT, 1 NB-NTN  */
#define NTN_BANDLOCK      "255"        /* Skylo L-band; "" disables lock  */
#define STATIC_LOCATION   0            /* 1 = inject fixed position       */
#define STATIC_LAT        "-37.8400"
#define STATIC_LON        "145.0800"
#define STATIC_ALT        "50"
#define UDP_HOST          "192.0.2.1"  /* collector endpoint - set me     */
#define UDP_PORT          9000
#define BACKHAUL_PERIOD_S 900
#define QUEUE_FLUSH_AT    16           /* early flush at this depth       */
#define DUTY_POWER_OFF    1            /* CFUN=0 between sessions         */

#define SOF        0xA5
#define MAX_FRAME  96
#define QUEUE_DEPTH 64
#define BATCH_MAX  1200

struct frame {
	uint8_t len;
	uint8_t data[MAX_FRAME];
};

K_MSGQ_DEFINE(uplink_q, sizeof(struct frame), QUEUE_DEPTH, 4);

static const struct device *const uart_dev =
	DEVICE_DT_GET(DT_ALIAS(bridge_uart));

static uint32_t stat_rx_ok, stat_rx_drop, stat_q_full, stat_sent, stat_fail;

/* ---------------- crc8 (poly 0x07, init 0x00, MSB first) -------------- */
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

/* ---------------- UART RX framing ---------------- */
enum rx_state { S_SOF, S_LEN, S_DATA, S_CRC };

static void uart_isr(const struct device *dev, void *user_data)
{
	static enum rx_state st = S_SOF;
	static struct frame f;
	static uint8_t idx;
	uint8_t b;

	ARG_UNUSED(user_data);

	if (!uart_irq_update(dev)) {
		return;
	}
	while (uart_irq_rx_ready(dev) && uart_fifo_read(dev, &b, 1) == 1) {
		switch (st) {
		case S_SOF:
			if (b == SOF) {
				st = S_LEN;
			}
			break;
		case S_LEN:
			if (b == 0 || b > MAX_FRAME) {
				stat_rx_drop++;
				st = S_SOF;
				break;
			}
			f.len = b;
			idx = 0;
			st = S_DATA;
			break;
		case S_DATA:
			f.data[idx++] = b;
			if (idx == f.len) {
				st = S_CRC;
			}
			break;
		case S_CRC: {
			uint8_t c = crc8(0x00, &f.len, 1);

			c = crc8(c, f.data, f.len);
			if (c == b) {
				if (k_msgq_put(&uplink_q, &f, K_NO_WAIT) == 0) {
					stat_rx_ok++;
				} else {
					stat_q_full++;
				}
			} else {
				stat_rx_drop++;
			}
			st = S_SOF;
			break;
		}
		}
	}
}

/* ---------------- modem ---------------- */
static int modem_sysmode_configure(void)
{
	int err;

#if NTN_ENABLE
	/* Fifth %XSYSTEMMODE parameter enables NB-NTN (NTN AT guide v1.2). */
	err = nrf_modem_at_printf("AT%%XSYSTEMMODE=0,0,0,0,1");
	if (err) {
		LOG_ERR("sysmode NTN failed: %d", err);
		return err;
	}
	if (sizeof(NTN_BANDLOCK) > 1) {
		err = nrf_modem_at_printf("AT%%XBANDLOCK=2,,\"%s\"",
					  NTN_BANDLOCK);
		if (err) {
			LOG_WRN("bandlock failed: %d", err);
		}
	}
#if STATIC_LOCATION
	/* Static UE position instead of a GNSS fix (fixed-site relay).
	 * VERIFY parameter semantics against NTN AT guide v1.2.
	 */
	err = nrf_modem_at_printf(
		"AT%%LOCATION=2,\"%s\",\"%s\",\"%s\",0,0",
		STATIC_LAT, STATIC_LON, STATIC_ALT);
	if (err) {
		LOG_WRN("static location failed: %d", err);
	}
#endif
#else
	err = nrf_modem_at_printf("AT%%XSYSTEMMODE=0,1,0,0");
	if (err) {
		LOG_ERR("sysmode NB-IoT failed: %d", err);
		return err;
	}
#endif
	return 0;
}

/* ---------------- backhaul session ---------------- */
static int session_run(void)
{
	static uint8_t batch[BATCH_MAX];
	struct frame f;
	size_t off = 5; /* "EWB1" + count */
	uint8_t count = 0;
	int64_t t0 = k_uptime_get();
	int sock, err;

	memcpy(batch, "EWB1", 4);

	while (count < 255 &&
	       off + 1 + MAX_FRAME <= BATCH_MAX &&
	       k_msgq_get(&uplink_q, &f, K_NO_WAIT) == 0) {
		batch[off++] = f.len;
		memcpy(&batch[off], f.data, f.len);
		off += f.len;
		count++;
	}
	if (count == 0) {
		return 0;
	}
	batch[4] = count;

	LOG_INF("session: %u frames, %u bytes, connecting...", count,
		(unsigned int)off);

	err = lte_lc_connect();
	if (err) {
		LOG_ERR("attach failed: %d (frames kept: requeue not possible, "
			"batch dropped: %u)", err, count);
		stat_fail++;
		goto power;
	}
	LOG_INF("attached in %lld ms", k_uptime_get() - t0);

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("socket: %d", -errno);
		stat_fail++;
		goto power;
	}

	struct sockaddr_in dst = {
		.sin_family = AF_INET,
		.sin_port = htons(UDP_PORT),
	};
	inet_pton(AF_INET, UDP_HOST, &dst.sin_addr);

	err = sendto(sock, batch, off, 0, (struct sockaddr *)&dst,
		     sizeof(dst));
	if (err < 0) {
		LOG_ERR("sendto: %d", -errno);
		stat_fail++;
	} else {
		stat_sent += count;
		LOG_INF("sent %d bytes, total %lld ms", err,
			k_uptime_get() - t0);
	}
	close(sock);

power:
#if DUTY_POWER_OFF
	lte_lc_power_off();
#else
	lte_lc_offline();
#endif
	return 0;
}

int main(void)
{
	int err;

	LOG_INF("EW cross-band bridge (%s mode)",
		NTN_ENABLE ? "NB-NTN" : "terrestrial NB-IoT");

	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("modem lib init: %d", err);
		return err;
	}
	err = modem_sysmode_configure();
	if (err) {
		return err;
	}

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("bridge uart not ready");
		return -ENODEV;
	}
	uart_irq_callback_user_data_set(uart_dev, uart_isr, NULL);
	uart_irq_rx_enable(uart_dev);
	LOG_INF("uart listening, period %d s, flush at %d frames",
		BACKHAUL_PERIOD_S, QUEUE_FLUSH_AT);

	for (;;) {
		for (int s = 0; s < BACKHAUL_PERIOD_S; s++) {
			if (k_msgq_num_used_get(&uplink_q) >= QUEUE_FLUSH_AT) {
				break;
			}
			k_sleep(K_SECONDS(1));
		}
		if (k_msgq_num_used_get(&uplink_q) == 0) {
			continue;
		}
		session_run();
		LOG_INF("stats rx_ok=%u drop=%u qfull=%u sent=%u fail=%u",
			stat_rx_ok, stat_rx_drop, stat_q_full, stat_sent,
			stat_fail);
	}
	return 0;
}

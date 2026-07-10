# EW Cross-Band Relay - Engineering Feasibility Pack

Prepared for Eden Worth Pty Ltd, Melbourne. Engineering only. No pricing, no
market sizing, no patent or prior-art work.

Date compiled: 2026-07-10. Author: Claude Code cloud session.
Spelling: British/Australian. Constraint set: AU915 / ACMA LIPD class licence,
30 dBm EIRP ceiling.

IMPORTANT: Every load-bearing number in this pack is either cited with a source
and date, or listed in Section 7 (VERIFY) as unproven. Do not quote a figure
from the body without checking it does not also carry a VERIFY flag. Several
Nordic datasheet PDFs were blocked (HTTP 403) through the session proxy within
budget, so some electrical figures could not be read first-hand and are flagged.

---

## 1. Executive verdict

### Variant A - Relay (SX1262 + TS011, nRF9151 sole host, Li-SOCl2, 10-year target)

Verdict: CONDITIONAL BUILD. The backhaul path is real: the nRF9151 gained
Skylo NB-IoT NTN certification in December 2025 and Nordic shipped the first
NTN modem firmware (v1.0.0) plus a supporting nRF Connect SDK release the same
month [Nordic, 2025-12-16; Nordic, 2025-12]. Over Australian territory a Skylo
GEO path (always in view) is the energy-sane backhaul for a primary-battery
relay; LEO operators (Sateliot, OQ Technology, Myriota) are licensed/launching
in AU but their store-and-forward, wait-for-pass duty cycle costs more energy
per delivered message. Two things stop this being an unconditional build and
both need a bench prototype, not more desk research: (1) whether one nRF9151
Cortex-M33 application core can host the real-time LoRa relay scheduler (TS011,
via LoRa Basics Modem on SX1262) and the NTN link-management stack without
timing collisions; and (2) whether measured energy per NTN session actually
closes a 10-year Li-SOCl2 budget. Build a single-core prototype and measure
session energy before committing the enclosure.

### Variant B - Gateway-Relay (SX1302/03 + Linux SoM + nRF9151 backhaul, solar + LiFePO4)

Verdict: CONDITIONAL BUILD, with one defining engineering task. The concentrator
side is mature and off-the-shelf: Semtech's SX1302/03 HAL and packet forwarder
are open source and support standard AU915 [Lora-net/sx1302_hal]. The hard,
non-negotiable finding is that neither the Semtech UDP packet forwarder nor
LoRa Basics Station is workable as-is over a 1-2 kbps GEO or intermittent LEO
NB-IoT backhaul - both assume an always-on, low-latency IP pipe with frequent
keepalives and (for Basics Station) a persistent TLS WebSocket. A custom
store-and-forward bridge between the concentrator and the network server is
therefore unavoidable and is the central firmware deliverable for this variant.
Solar + LiFePO4 removes the battery-life pressure that dominates Variant A, so
the risk here is software effort and NS-side integration, not power.

---

## 2. NTN operator / engineering matrix (Australia)

All rows concern 3GPP NB-IoT NTN unless noted. "nRF9151 status" = certification
or demonstration status of the nRF9151 specifically, not the operator's network
readiness.

| Operator | Orbit | Band | Live over AU? | Delivery model | nRF9151 status | Source (date) |
|---|---|---|---|---|---|---|
| Skylo | GEO | L-band (via Viasat/Inmarsat, plus Ligado/TerreStar/EchoStar) | Yes - AU/NZ named in coverage, launching | GEO always-in-view; near-real-time, narrowband | CERTIFIED (module) | Nordic 2025-12-16; Skylo newsroom 2025 |
| Sateliot | LEO | 3GPP Rel-17 NB-IoT NTN (5G) | Licensed in AU; commercial roll-out from H2 2025 | Store-and-forward (pass-based) | Nordic collaboration / demo, not stated certified | Sateliot 2025-05-13; techpartner.news 2025 |
| OQ Technology | LEO (10 sats in orbit) | 3GPP NB-IoT | AU subsidiary opened 2025-10-01, service to critical sectors | Store-and-forward (pass-based) | Direct NTN LEO connectivity demonstrated with Nordic | GSMA 2025; Nordic 2025-12 |
| Myriota (HyperPulse) | LEO | 3GPP Rel-17 NB-IoT NTN (since 2026 hybrid; AU-based) | Yes - AU in coverage list | Historically store-and-forward; now hybrid sat/cellular | Not stated for nRF9151 specifically | SatNews / IoT Business News 2026-07-01 |
| Iridium NTN Direct | LEO | L-band, 3GPP Rel-19 | Global (incl. AU) once live | Rel-19 D2D/NB-IoT | Goes live 2026; nRF9151 is a Rel-17 NTN device, so Rel-19 support is NOT expected - VERIFY | Iridium services page (2026 target) |

Engineering reading: for a battery relay the only presently certified nRF9151
path is Skylo GEO. GEO gives a continuous, if very narrow, link, which is the
right fit for a duty-cycled sensor backhaul and avoids the pass-scheduling and
extended-idle energy cost of LEO. Treat Sateliot/OQ/Myriota LEO as a secondary
or future path pending nRF9151 certification on each, and treat Iridium NTN
Direct as out of scope for the current Rel-17 nRF9151 silicon until proven.

---

## 3. Firmware feasibility per variant

### 3.0 nRF9151 NTN modem firmware maturity (shared)

- First NTN modem firmware released December 2025, versioned v1.0.0 and aligned
  with the nRF91x1 NTN AT Commands Reference Guide v1.0. Adds NB-IoT NTN
  (3GPP Rel-17) alongside terrestrial LTE-M/NB-IoT and GNSS, and supports both
  GEO and LEO [Nordic, 2025-12].
- Nordic stated the commercial (production) firmware release was scheduled for
  early 2026 pending further certifications, so as at brief date the FW is at
  or just past its first production release and should be treated as early-life,
  not battle-hardened [Nordic/PRNewswire, 2025-12].
- nRF Connect SDK v3.2.0 adds nRF9151 SMA DK support, a dedicated NTN helper
  library, an `ntn` command, and NTN NB-IoT options in the `link sysmode` and
  `link edrx` commands [NCS v3.2.0 release notes]. PSM/eDRX are exposed for NTN;
  a maintenance note mentions delaying IPv6 updates until after PSM sleep for
  power efficiency.
- NOT established within budget, hence VERIFY: whether transport is NIDD
  (non-IP data delivery, control-plane) or IP/UDP over NTN and which operators
  require which; how store-and-forward for LEO passes is surfaced to the
  application (modem-buffered vs application-managed); NTN PSM/eDRX timer
  behaviour and floor sleep current; and any NTN-specific errata in v1.0.0.

### 3.1 Variant A firmware feasibility

Scope: one nRF9151 application core (Cortex-M33, ~1 MB flash / 256 kB RAM) hosts
both the LoRa relay (SX1262 + LoRa Basics Modem, TS011 relay build) and the
NB-IoT NTN backhaul link management.

Findings:
- LoRa Basics Modem is delivered inside Semtech's Unified Software Platform (USP)
  and there is a Zephyr integration (Lora-net/usp_zephyr) covering SX126x, and
  Zephyr's LoRa backend exposes LoRa Basics Modem for SX1261/SX1262
  [Lora-net/usp_zephyr; Zephyr LoRa/LoRaWAN docs]. So the SX1262 driver and LBM
  on a Cortex-M33 under Zephyr/NCS is a supported starting point.
- TS011-1.0.0 (LoRa Alliance) defines the relay mechanism that transports frames
  bi-directionally between an end-device and gateway/NS [LoRa Alliance TS011].
  The relay is a specific build/feature set; its maturity and availability as an
  open, nRF-portable build (as opposed to a Semtech reference on Semtech radios)
  is not confirmed within budget - VERIFY.
- Footprint is not the constraint: LBM is tens of kB of flash and modest RAM,
  comfortably inside 1 MB / 256 kB even alongside the NTN helper library.

Top FW risk (named): single-core real-time coexistence. The TS011 relay must
service tight LoRa RX windows and relay timing on the same M33 that runs NTN
link management (long timing advance, NTN-scale timers, PSM/eDRX state machine,
and any GNSS fix needed for Doppler/pointing). A blocking NTN operation or an
ill-timed modem event that stalls the LoRa scheduler will drop relayed frames.
This is a scheduling/priority problem that desk research cannot close; it needs
a two-radio prototype with the LoRa RX-window jitter measured while the NTN
stack is actively synchronising and transmitting. Note the brief pins nRF9151 as
the *sole* host, so there is no second core to offload the LoRa timing to.

### 3.2 Variant B firmware feasibility

Scope: SX1302/03 concentrator + small Linux SoM + nRF9151 backhaul, for
unmodified AU915 devices.

Findings:
- Semtech SX1302/03 HAL, packet forwarder, and CoreCell reference (SPI/USB) are
  open source and support AU915; Basics Station is also supported on SX1302/03
  SPI/USB concentrators [Lora-net/sx1302_hal]. This is mature and low-risk on
  the RF/concentrator side.
- Host: the concentrator plus a store-and-forward bridge is light compute. A
  small Cortex-A Linux SoM is sufficient - candidate classes: Raspberry Pi CM4,
  NXP i.MX 8M Mini SoM, or a Variscite/Digi i.MX-class SoM. Selection should be
  driven by the SPI/USB concentrator interface, Linux LTS support, and idle
  power under the solar budget, not by raw compute - VERIFY exact SoM once the
  bridge software footprint is known.
- Backhaul transport reality: the Semtech UDP packet forwarder sends periodic
  PULL_DATA keepalives and expects prompt PUSH/PULL ACKs; Basics Station holds a
  persistent TLS WebSocket to the LNS. Both assume an always-on, low-latency IP
  link. Over a 1-2 kbps GEO or intermittent LEO NB-IoT backhaul the keepalive
  cadence, JSON overhead, per-packet ACKing and TLS session cost are all
  prohibitive [derived from Lora-net/sx1302_hal packet_forwarder/Basics Station
  protocol behaviour].

Top FW risk (named): the standard forwarders do not survive the backhaul, so a
custom store-and-forward bridge between the concentrator and the network server
is unavoidable. It must locally buffer uplinks, batch and compress them, tolerate
long link outages, manage downlink latency expectations, and present something
the NS accepts (either a thin gateway shim or an application-layer feed). This
is the defining engineering task of Variant B, not an optimisation.

---

## 4. RF / power hardware findings

- Output power: nRF9151 supports 3GPP power class 3 (23 dBm) and power class 5
  (20 dBm); Nordic cites a ~45% peak-power reduction versus prior parts at class
  3 [Nordic 2024-09; product brief]. At AU915 with the 30 dBm EIRP ceiling, the
  LoRa side has generous antenna-gain headroom; the NTN side is a separate L-band
  path governed by the operator's licence, not the LIPD class licence.
- TX current: the exact peak/average TX current at 23 dBm could not be read
  first-hand within budget (nRF9151 Product Specification v1.0, 2024-07-29, and
  two other datasheet PDFs returned HTTP 403 through the proxy). Family-level
  Nordic parts put NB-IoT peak TX in the low-hundreds of mA at ~3.0-3.7 V; the
  precise figure is in VERIFY and must be read from the product specification
  before the power model is trusted.
- Antenna / coexistence: three RX/TX paths must share one enclosure - 915 MHz
  LoRa omni (TX+RX), L-band NTN (~1.6 GHz), and GNSS L1 (1575.42 MHz). The NTN
  L-band RX and GNSS L1 sit within ~tens of MHz of each other, so both need
  good front-end selectivity and physical separation. The 915 MHz LoRa TX is the
  aggressor: its second harmonic lands near 1830 MHz and its broadband transmit
  noise can desense the L-band NTN and GNSS receivers. Mitigation: SAW/ceramic
  filtering on the L-band and GNSS front ends, a harmonic filter on the 915 MHz
  PA output, physical antenna separation, and time-division so 915 TX never
  overlaps an NTN or GNSS receive window. Quantified isolation figures are in
  VERIFY.
- Energy store (Variant A): Li-SOCl2 has high energy density but poor pulse
  capability, so a hybrid-layer capacitor or supercap must buffer the NTN TX
  current bursts (low-hundreds of mA) that the cell alone cannot supply without
  voltage sag. HLC sizing follows directly from the per-session peak current and
  burst duration, both of which are in VERIFY pending measurement.
- Energy per session: GEO (Skylo) sessions are short and predictable because the
  satellite is always in view; LEO sessions incur wait-for-pass and extended
  connected/idle time, costing more energy per delivered message. This is the
  main reason Variant A should default to GEO backhaul.

---

## 5. Power / device-ceiling model (with stated assumptions)

This is a transparent method, not a measured result. Every input marked (A) is an
assumption to be replaced with a bench figure; energy-per-session (A3) is the
single most important VERIFY.

Assumptions:
- (A1) Li-SOCl2 pack: 2x D-cell primary, ~38 Ah usable at ~3.6 V nominal.
- (A2) 10-year target with ~20% derating for self-discharge/temperature, so
  usable ~30 Ah over 10 years => average budget ~= 30000 mAh / 3650 days
  ~= 8.2 mAh/day for the whole device (relay housekeeping + LoRa RX + NTN).
- (A3) Energy per Skylo GEO NTN session (connect + short uplink + GNSS-assisted
  Doppler support + teardown): assumed 0.6 mAh/session for the model. THIS IS THE
  KEY UNVERIFIED INPUT.
- (A4) LoRa relay RX/housekeeping average: assumed 2 mAh/day.
- (A5) Served nodes each send 1 uplink/hour, ~20-byte payload (24 msg/day/node).
- (A6) Relay aggregates all served-node traffic per backhaul window (batching),
  so served-node count drives NTN payload size, not session count, until a
  session's airtime cap is hit.

Worked model (Variant A, GEO):
- Daily energy left for NTN after housekeeping: 8.2 - 2 (A4) = ~6.2 mAh/day.
- Sessions/day affordable: 6.2 / 0.6 (A3) ~= 10 backhaul sessions/day, i.e. a
  backhaul roughly every 2.4 hours if energy-limited.
- Served-device ceiling is then set by how much aggregated payload fits in those
  sessions within NB-IoT NTN airtime and the operator's message-size limits, not
  by relay energy. With hourly 20-byte uplinks batched into ~10 sessions/day, a
  ceiling in the low tens of nodes is plausible and consistent with the
  comparable ProEsys relay's stated 16-device capacity - but the binding limit is
  NTN uplink throughput/airtime per session, which is in VERIFY.

Sensitivity: the model is dominated by (A3). If real session energy is 1.5 mAh
(2.5x the assumption), affordable sessions drop to ~4/day and either cadence
stretches to ~6 hours or the pack must grow. If it is 0.3 mAh, backhaul can be
near-hourly. No device-ceiling claim should be quoted until (A3) is measured on
a prototype against a live Skylo GEO link.

Variant B: solar + LiFePO4 removes the primary-battery ceiling; the constraints
become solar/panel/battery sizing for the Linux SoM idle draw and concentrator
duty, plus NTN airtime per backhaul window. Not battery-life limited, so no
10-year model applies.

---

## 6. Competitor architecture table (engineering only)

| Product | Architecture | Bands | Relay vs Gateway | Status | Source (date) |
|---|---|---|---|---|---|
| ProEsys Crossband IoT Relay (ALPHA-G1-UM) | LoRaWAN relay backhauled over satellite via EchoStar Mobile EM2050 module | ISM (868/915) + S-band satellite | Relay (extends LoRaWAN to sat); up to 16 devices; 10yr battery or solar | Commercial | ProEsys / LoRa Alliance marketplace 2025 |
| Kerlink Wirnet iStation | Outdoor LoRaWAN gateway, terrestrial backhaul (4G with 2G/3G fallback, Ethernet), integrated GPS | ISM LoRa + cellular | Gateway (terrestrial; not satellite natively) | Commercial | Kerlink / TTI docs 2019-2025 |
| Kineis (as backhaul, e.g. via Kerlink integrations) | LEO store-and-forward satellite IoT, KIM modules | UHF (~400 MHz) | Neither LoRaWAN nor NB-IoT; separate air interface | Operational LEO | GSMA/industry 2025 |
| Lacuna Space | LoRa/LR-FHSS space gateway on LEO nanosats, direct-to-device from ground nodes | ISM LoRa uplink / LR-FHSS | Gateway-in-space (D2D), store-and-forward | Constellation deploying | eoPortal / Lacuna 2025 |
| Wyld Networks (Wyld Connect) | LoRaWAN direct-to-satellite modules to LEO partners | ISM LoRa | Direct-to-satellite device (store-and-forward) | Commercial modules | Wyld / industry 2025 |

Key engineering distinction versus the EW concept: ProEsys backhauls over an
EchoStar S-band LoRaWAN satellite module (a LoRaWAN-over-satellite path), whereas
both EW variants backhaul over 3GPP NB-IoT NTN through the nRF9151. That is a
different radio, a different regulatory/spectrum regime, and a different firmware
stack - the competitor's relay is not a drop-in reference for the NTN link layer.

---

## 7. VERIFY list - checkpoints requiring Dale's factual sign-off

Firmware / NTN:
1. nRF9151 NTN transport model: NIDD (control-plane, non-IP) vs IP/UDP, per
   operator, and which the v1.0.0 modem FW supports over Skylo GEO.
2. How LEO store-and-forward is surfaced to the application (modem-buffered vs
   app-managed retry/queue) in the current modem FW.
3. NTN PSM/eDRX timer ranges and floor sleep current in NTN mode.
4. NTN-specific errata in modem FW v1.0.0 and whether the production ("early
   2026") release changes any of the above.
5. TS011 relay build availability and maturity as an nRF9151-portable target
   (open LoRa Basics Modem/USP build, not just a Semtech-radio reference).
6. Measured LoRa RX-window jitter on the nRF9151 M33 while the NTN stack is
   synchronising and transmitting (the single-core coexistence risk, Variant A).
7. Confirmation that no standard forwarder (Semtech UDP PF / Basics Station) can
   be tuned to survive the NTN backhaul, sizing the custom bridge scope
   (Variant B).

RF / power:
8. nRF9151 peak and average TX current at 23 dBm (NB-IoT and LTE-M), supply
   voltage, and PSM/eDRX sleep current - read from nRF9151 Product Specification
   v1.0 (2024-07-29); datasheet PDFs were 403-blocked this session.
9. Measured energy per Skylo GEO NTN session end-to-end (input A3 of the model).
10. NTN uplink throughput / airtime and per-message size limits per operator
    (sets the true served-device ceiling).
11. Antenna isolation and desense budget: 915 MHz TX (and its 1830 MHz harmonic)
    into L-band NTN RX and GNSS L1, with the filtering/separation needed to meet
    NTN and GNSS sensitivity.
12. HLC/supercap sizing from measured NTN TX burst current and duration.
13. Operator certification of nRF9151 specifically on Sateliot, OQ Technology and
    Myriota (only Skylo is confirmed certified as at 2025-12).
14. Whether Iridium NTN Direct (Rel-19) can ever be served by the Rel-17 nRF9151
    silicon (expected no; confirm).

Device-ceiling model:
15. Replace assumptions A1-A6 with measured/spec values, re-run Section 5, and
    fix the served-device ceiling per variant.

---

## 8. Budget consumed

- Web searches/fetches used: 14 of the 20 allowed (11 WebSearch queries plus
  3 WebFetch attempts; all 3 fetches of Nordic/distributor datasheet PDFs
  returned HTTP 403 through the session proxy and yielded no content).
- These blocked fetches are the direct cause of VERIFY item 8 (exact TX current);
  the figure exists in the cited product specification but could not be read
  first-hand within budget.
- Time: within the 40-minute elapsed limit (session start 02:19 UTC).
- Research tasks 1-7 in the brief were all addressed. No task was dropped for
  budget; depth on electrical figures was limited by the PDF blocks, not by the
  search count.

Searches used (queries):
1. nRF9151 NB-IoT NTN modem firmware / Skylo certification 2025
2. Skylo NTN Australia coverage / GEO / Inmarsat L-band / store-and-forward
3. Sateliot / OQ Technology NB-IoT NTN LEO Australia status
4. nRF9151 NTN modem firmware version / NIDD / PSM / eDRX / NCS release notes
5. LoRa Basics Modem / TS011 relay / Zephyr / nRF9151 / SX1262 footprint
6. Semtech SX1302/03 HAL / Basics Station / packet forwarder / Linux host
7. nRF9151 TX current 23 dBm / power profiler / datasheet
8. ProEsys Crossband IoT Relayer architecture / bands
9. Kerlink iStation / Kineis / Lacuna Space / Wyld Networks architecture
10. Myriota / Iridium NTN Direct NB-IoT status 2025-2026
11. nRF9151 peak TX current / PSM sleep / power class 3 (specifics)
Fetch attempts (all 403): Nordic Dec-2025 launch page; emcraft nRF9151 power PDF;
Mouser nRF9151 Product Specification PDF.

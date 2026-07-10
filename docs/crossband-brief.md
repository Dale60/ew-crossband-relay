# EW Cross-Band Relay - Feasibility Brief

Execution brief for a Claude Code cloud session. Engineering only.

## GOAL

Produce an engineering feasibility pack (hardware + firmware) for a cross-band
LoRaWAN-to-satellite relay ("EW Cross-Band Relay") built on the Nordic nRF9151
NB-IoT NTN backhaul, benchmarked against the ProEsys Crossband IoT Relayer.
Output must be decision-ready: build / don't-build per variant, with every
unverified claim flagged. Engineering only - no pricing, no market sizing,
no patent/prior-art work.

## USAGE LIMIT

Hard budget: 40 minutes elapsed OR 20 web searches/fetches, whichever first.
Track both and report them. If the budget is reached, stop and deliver
findings to date plus an explicit list of gaps. Do not exceed the budget.

## CONTEXT

- Eden Worth Pty Ltd, Melbourne. Existing nRF9151/nRF52840 hardware and
  nRF Connect SDK/Zephyr firmware experience (Optima platform).
- Target deployment: Australian remote infrastructure (cathodic protection,
  rail, water metering black spots, bushfire sensing).
- Two candidate variants:
  - A) Relay: SX1262 + TS011 relay stack, nRF9151 app core as sole host,
    Li-SOCl2 + hybrid-layer cap, 10-year battery target, served nodes
    run the relay stack (LoRa Basics Modem).
  - B) Gateway-Relay: SX1302/03 concentrator + small Linux compute +
    nRF9151 backhaul, solar + LiFePO4, supports unmodified AU915 devices.

## RESEARCH TASKS (priority order - drop from the bottom if budget tightens)

1. NTN service gating (slim): for Skylo, Sateliot, OQ Technology, Myriota,
   Iridium NTN Direct - is NB-IoT NTN live or near-term over Australian
   territory, which band, GEO or LEO, store-and-forward or real-time, and
   nRF9151 certification status per operator. No pricing. Cite dates.
2. nRF9151 NTN firmware maturity: current NTN modem FW version and GA/alpha
   status, Rel-17 feature coverage, PSM/eDRX behaviour over NTN, NIDD vs
   IP/UDP transport, store-and-forward handling, required nRF Connect SDK
   version, known errata affecting NTN.
3. Variant A firmware feasibility: LoRa Basics Modem (TS011 relay build) on
   the nRF9151 Cortex-M33 - Zephyr/NCS port status, SX1262 driver maturity,
   flash/RAM footprint vs 1 MB / 256 kB, scheduler coexistence with LTE/NTN
   link management on the same core. Identify the single biggest FW risk.
4. Variant B firmware feasibility: SX1302 HAL and Basics Station host
   requirements, minimum viable Linux SoM candidates, and critically -
   whether standard Semtech UDP/Basics Station forwarding is workable over
   a 1-2 kbps GEO or intermittent LEO link, or whether a custom
   store-and-forward bridge between concentrator and NS is unavoidable.
5. RF and power hardware: nRF9151 TX current profile at 23 dBm NTN, energy
   per GEO and LEO session, antenna selection for 915 MHz omni + L-band NTN
   + GNSS L1 in one enclosure, coexistence/desense risk of 915 MHz TX into
   GNSS and L-band RX paths and filtering implications, Li-SOCl2 + HLC/
   supercap sizing for NTN TX bursts.
6. Power/device-ceiling model: devices served x uplink cadence x payload
   size -> daily backhaul airtime and energy -> battery life (Variant A)
   and realistic served-device ceiling per variant. State all assumptions.
7. Competitor architecture teardown (brief, engineering only): ProEsys
   Crossband Relayer, Kerlink iStation M2 (Kineis), Lacuna Space, Wyld
   Networks - one line each: architecture, bands, relay vs gateway, status.

## CONSTRAINTS

- AU915 / ACMA LIPD class licence assumptions throughout; 30 dBm EIRP ceiling.
- British/Australian spelling. No marketing language. No em dashes.
- Every factual claim carries a source + date. Anything not verified within
  budget goes in a "VERIFY" list, not in the body as fact.

## OUTPUT

Write the pack to docs/crossband-feasibility.md on a new branch named
feasibility-pack and commit it. Structure:

1. Executive verdict (build/don't-build per variant, one paragraph each)
2. NTN operator/engineering matrix (AU)
3. FW feasibility findings per variant, each with top risk named
4. RF/power hardware findings
5. Power/device-ceiling model with stated assumptions
6. Competitor architecture table
7. VERIFY list - checkpoints requiring Dale's factual sign-off
8. Budget consumed (time + search count)

If network access to research domains is blocked, stop immediately and
report which domains failed rather than filling gaps from memory.

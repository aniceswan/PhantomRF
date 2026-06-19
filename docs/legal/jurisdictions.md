# Jurisdictions

> A per-country summary of the legal picture for radio
> transmission. **This is not legal advice.** Check with
> your local regulator before transmitting.

The PhantomRF project does its best to keep this table
current, but laws change. If your country is missing or
the entry looks stale, please open an issue on GitHub.

---

## Table of contents

- [How to read this table](#how-to-read-this-table)
- [ITU Region 1 (Europe, Africa, Middle East)](#itu-region-1-europe-africa-middle-east)
- [ITU Region 2 (Americas)](#itu-region-2-americas)
- [ITU Region 3 (Asia-Pacific)](#itu-region-3-asia-pacific)
- [Offshore / international waters](#offshore--international-waters)
- [Next steps](#next-steps)

---

## How to read this table

For each country, we list:

- The **regulator** (the agency that enforces the law).
- The **transmit licence requirement** for the 2.4 GHz and
  sub-GHz bands.
- The **maximum power** allowed for unlicensed devices.
- **Key restrictions** (e.g. "DFS required for 5 GHz").
- **Penalty** for unlicensed transmission (typical).

The entries are summarised; the full text of each country's
rules is much longer. The links at the end of each section
go to the primary source.

## ITU Region 1 (Europe, Africa, Middle East)

### European Union (CEPT/ETSI)

- **Regulator:** national (e.g. BNetzA in Germany, ARCEP
  in France, Ofcom in the UK, AGCOM in Italy)
- **Licence:** Generally required for transmission in the
  ISM bands. Unlicensed devices are limited to specific
  bands and powers (see below).
- **2.4 GHz:** Yes, unlicensed under
  [ETSI EN 300 328](https://www.etsi.org/deliver/etsi_en/300300_300399/300328/).
  Maximum EIRP: 20 dBm (100 mW). Must use the
  "non-interference, non-protection" basis (you accept
  all interference; you must not cause interference).
- **Sub-GHz (433 MHz):** Yes, unlicensed under
  [ETSI EN 300 220](https://www.etsi.org/deliver/etsi_en/300300_300399/300220/).
  Maximum EIRP: 10 dBm (10 mW). Duty cycle ≤ 10%.
- **Sub-GHz (868 MHz):** Yes, unlicensed, but with
  sub-band restrictions and duty cycle limits.
- **Sub-GHz (915 MHz):** Not allocated in EU.
- **Penalty:** Fines up to €500,000 and/or 5 years
  imprisonment in some EU countries. Equipment seizure.

> :warning: **The EU rules are strict about "intentional
> interference".** Even with a valid licence, causing
> harmful interference is illegal. The PhantomRF's
> deauth, jam, and beacon flood modes are **all**
> "intentional interference" under EU law.

### United Kingdom (post-Brexit)

- **Regulator:** [Ofcom](https://www.ofcom.org.uk/)
- **Licence:** Similar to EU. 2.4 GHz at < 100 mW EIRP
  is licence-free under the "Interface Requirement
  2008" (IR2008). Sub-GHz bands are restricted.
- **Penalty:** Fines up to £5,000 and/or 6 months
  imprisonment for unlicensed transmission. Higher
  penalties for interference with safety services.

### Germany (BNetzA)

- **Regulator:** [Bundesnetzagentur](https://www.bundesnetzagentur.de/)
- Same as EU. Additional restrictions on 5 GHz (DFS
  required).
- **Penalty:** Up to €500,000 and/or 5 years
  imprisonment for unlicensed transmission.

### France (ANFR / ARCEP)

- **Regulator:** [ANFR](https://www.anfr.fr/) /
  [ARCEP](https://www.arcep.fr/)
- Same as EU. Strict enforcement.
- **Penalty:** Up to 6 months imprisonment and €30,000
  fine for unlicensed transmission.

### Russia

- **Regulator:** [Roskomnadzor](https://rkn.gov.ru/)
- 2.4 GHz is allowed for unlicensed devices at < 100 mW
  EIRP. Sub-GHz is restricted. **Strict** enforcement
  against "unauthorised" radio equipment.
- **Penalty:** Fines and equipment seizure; criminal
  charges for "illegal use of radio frequencies".

### South Africa

- **Regulator:** [ICASA](https://www.icasa.org.za/)
- 2.4 GHz is allowed at < 100 mW EIRP. Sub-GHz is
  restricted.
- **Penalty:** Up to 5 years imprisonment for unlicensed
  transmission.

### United Arab Emirates

- **Regulator:** [TDRA](https://www.tdra.gov.ae/)
- 2.4 GHz is allowed at < 100 mW EIRP for unlicensed
  devices. Sub-GHz is restricted.
- **Penalty:** Up to 2 years imprisonment and AED
  200,000 fine.

## ITU Region 2 (Americas)

### United States (FCC)

- **Regulator:** [Federal Communications Commission](https://www.fcc.gov/)
- 2.4 GHz is allowed under
  [Part 15](https://www.fcc.gov/part-15-unlicensed-radio-frequency-devices)
  at < 1 W conducted (4 W EIRP) for "intentional
  radiators". Sub-GHz is allowed under Part 15 at lower
  powers (see below).
- **Sub-GHz (315 MHz):** Allowed for "low-power
  communication devices" at < 0.1 W EIRP.
- **Sub-GHz (433 MHz):** Allowed at < 0.1 W EIRP under
  Part 15.231.
- **Sub-GHz (902–928 MHz):** Allowed under
  [Part 15.247](https://www.fcc.gov/document/15-247-operation-within-bands-902-928-mhz-240024835-mhz-and-57255850)
  at < 1 W conducted, with frequency-hopping or
  digital-modulation requirements.
- **Penalty:** Up to $10,000 per violation per day, plus
  equipment seizure. Criminal charges for "willful"
  violations.

> :warning: **FCC Part 15 explicitly prohibits "intentional
> interference".** A WiFi deauth is interference, even if
> the transmit power is within Part 15 limits. The "no
> interference, no protection" clause applies.

### Canada (ISED)

- **Regulator:** [Innovation, Science and Economic
  Development Canada](https://www.ic.gc.ca/)
- 2.4 GHz is allowed under
  [RSS-247](https://www.ic.gc.ca/eic/site/smt-gst.nsf/eng/sf10759.html)
  at < 4 W EIRP. Sub-GHz is allowed at lower powers.
- **Penalty:** Up to $25,000 per violation for individuals,
  $500,000 for corporations, plus equipment seizure.

### Mexico (IFT)

- **Regulator:** [Instituto Federal de Telecomunicaciones](https://www.ift.org.mx/)
- 2.4 GHz is allowed at < 200 mW EIRP for unlicensed
  devices. Sub-GHz is restricted.
- **Penalty:** Fines and equipment seizure.

### Brazil (Anatel)

- **Regulator:** [Anatel](https://www.gov.br/anatel/)
- 2.4 GHz is allowed at < 1 W EIRP. Sub-GHz is
  restricted.
- **Penalty:** Fines up to R$ 50 million for serious
  violations; equipment seizure.

## ITU Region 3 (Asia-Pacific)

### Japan (MIC / MIC)

- **Regulator:** [Ministry of Internal Affairs and Communications](https://www.soumu.go.jp/english/)
- 2.4 GHz is allowed at < 10 mW/MHz EIRP (so about
  200 mW for the full 20 MHz WiFi channel) for
  "construction design certified" devices. Sub-GHz is
  restricted.
- **Penalty:** Up to 1 year imprisonment and 1,000,000
  yen fine for unlicensed transmission.

### Australia (ACMA)

- **Regulator:** [Australian Communications and Media Authority](https://www.acma.gov.au/)
- 2.4 GHz is allowed under the "Low Interference
  Potential Devices" class at < 4 W EIRP. Sub-GHz is
  allowed at < 1 W EIRP for the 915–928 MHz band.
- **Penalty:** Fines up to AUD $1.1 million for
  individuals, $11.1 million for corporations.

### New Zealand (RSM)

- **Regulator:** [Radio Spectrum Management](https://www.rsm.govt.nz/)
- 2.4 GHz is allowed at < 4 W EIRP. Sub-GHz is
  restricted.
- **Penalty:** Fines up to NZD $200,000 and/or 5 years
  imprisonment.

### China (MIIT)

- **Regulator:** [Ministry of Industry and Information Technology](https://www.miit.gov.cn/)
- 2.4 GHz is allowed at < 100 mW EIRP. Sub-GHz is
  restricted.
- **Penalty:** Strict enforcement; fines and
  imprisonment for unlicensed transmission.

### India (WPC)

- **Regulator:** [Wireless Planning and Coordination Wing](https://wpc.dot.gov.in/)
- 2.4 GHz is allowed at < 1 W EIRP for unlicensed
  devices. Sub-GHz is restricted to licensed users.
- **Penalty:** Fines up to INR 100,000 and/or 3 years
  imprisonment.

### Singapore (IMDA)

- **Regulator:** [Infocomm Media Development Authority](https://www.imda.gov.sg/)
- 2.4 GHz is allowed at < 100 mW EIRP. Sub-GHz is
  restricted.
- **Penalty:** Fines up to SGD $10,000 and/or 3 years
  imprisonment.

### South Korea (MSIT)

- **Regulator:** [Ministry of Science and ICT](https://www.msit.go.kr/)
- 2.4 GHz is allowed at < 10 mW/MHz EIRP. Sub-GHz is
  restricted.
- **Penalty:** Fines up to KRW 5,000,000 and/or 5 years
  imprisonment.

## Offshore / international waters

International waters (more than 12 nautical miles from any
coast) are governed by ITU international rules. The relevant
convention is the [ITU Radio Regulations](https://www.itu.int/en/myitu/Publications/2021/09/02/02/04/Radio-Regulations-Edition-of-2020).

In practice:

- The 2.4 GHz ISM band is allocated worldwide for
  unlicensed use, but the **transmit power limits** vary.
- The sub-GHz bands are allocated differently in each
  ITU region.
- Your flag state (the country whose flag your vessel
  flies) is responsible for enforcing the rules on your
  vessel.

If you're on a research vessel, contact the vessel's
flag-state administration for guidance.

---

## Disclaimer reminder

This page is **summary information only**. It is not legal
advice. For any serious use of PhantomRF, consult a lawyer
in your jurisdiction who specialises in telecommunications
law.

The PhantomRF project is not responsible for the accuracy
of this table, and the laws may have changed since this
page was last updated. Check the regulator's website for
the current rules.

---

## Next steps

- The ethical-use case list is in
  [Ethics](ethics.md).
- The full disclaimer is in
  [Disclaimer](disclaimer.md).
- The first-build instructions are in
  [Installation](../getting-started/installation.md).

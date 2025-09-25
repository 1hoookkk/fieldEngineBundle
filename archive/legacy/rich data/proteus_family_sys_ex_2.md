# Proteus Family SysEx 2.2 — Quick Reference

For: Proteus 2000 / 2500, Audity 2000, XL‑7, MP‑7, PX‑7, PK‑6, MK‑6, XK‑6, Vintage Keys/Pro, Halo.

---

## Global Format & Header
- **Standard Data Format:** `F0 18 0F <devID> 55 <Command> [<Count>] <Packets…> [<Checksum>] F7`
- **Manufacturer/Family:** `18` (E‑MU), `0F` (Proteus family)
- **Device ID:** `00–7E` unique, `7F` broadcast
- **Editor designator:** `55`
- **Checksum:** one‑byte 1’s complement of the sum of *data bytes* (some messages set `7F` to ignore)

### Universal (MMA) Non‑Real‑Time examples
- **Identity Request:** `F0 7E <devID> 06 01 F7`
- **Identity Reply:** `F0 7E <devID> 06 02 18 04 04 <family member LSB,MSB> <rev ASCII×4> F7`
- **Bulk Tuning Dump Req:** `F0 7E <devID> 08 00 <tt> F7`
- **Bulk Tuning Dump:** `F0 7E <devID> 08 01 <tt> <16 ASCII name> [<xx yy zz>×128] <checksum> F7`
- **Single Note Tuning Change:** `F0 7E <devID> 08 02 <tt> <ll> [<kk xx yy zz>]… F7`
- **Master Volume (Device Control):** `F0 7E <devID> 04 01 <vv lsb> <vv msb> F7`

---

## Parameter Edit / Query
- **Edit value(s):**
  - `F0 18 0F <devID> 55 01 <count> [<paramID LSB,MSB> <data LSB,MSB>]… F7`
  - Avoid >244 data bytes (~41 edits) in one packet.
- **Request value(s):**
  - `F0 18 0F <devID> 55 02 <count> [<paramID LSB,MSB>]… F7`
  - Unit replies with one complete **Edit** message per parameter.
- **Min/Max/Default (response):** `… 03 <paramID> <min> <max> <default> <roFlag> F7`
- **Min/Max/Default Request:** `… 04 <paramID> F7`

---

## Configuration & Names
- **Hardware Config Request:** `… 0A F7` → Response `… 09 … F7` includes: user preset count; SIMM count/IDs; per‑SIMM preset/instrument counts.
- **Generic Name Request:** `… 0C <type> <obj#> <romID> F7`
  - Types: `1=Preset, 2=Instrument, 3=Arp, 4=Setup, 5=Demo, 6=Riff`
- **Generic Name Response:** `… 0B <type> <obj#> <romID> <16 ASCII> F7`

---

## Preset Dumps (Closed/Open Loop)
**Important:** Only **one preset** dump/restore at a time.

### Header
- `… 10 01|03 <preset#> <dataBytes 32b> <#PCGen> <#Reserved> <#PCEfx> <#PCLinks> <#Layers> <#LayGen> <#LayFilt> <#LayLFO> <#LayEnv> <#LayCords> <ROM ID> F7`

### Data Packets
- `… 10 02|04 <packet# 14b> <up to 244 data> <checksum> F7`

### Sectional Dumps (selected subcommands)
- **Preset Common (all):** `… 10 10 <240 data> F7`
- **Preset Common General:** `… 10 11 <126 data> F7`
- **Preset Common Arp:** `… 10 12 <38 data> F7`
- **Preset Common FX:** `… 10 13 <38 data> F7`
- **Preset Common Links:** `… 10 14 <46 data> F7`
- **Layer (all):** `… 10 20 <332 data> F7`
- **Layer General:** `… 10 21 <70 data> F7`
- **Layer Filter:** `… 10 22 <14 data> F7`
- **Layer LFO:** `… 10 23 <28 data> F7`
- **Layer Env:** `… 10 24 <92 data> F7`
- **Layer PatchCords:** `… 10 25 <152 data> F7`

### Requests mirror with command `… 11 <subcmd> … F7`
- Examples: Preset Dump Req (`02` closed / `04` open), Preset Common (`10`), Layer (`20–25`).

---

## Program Change ↔ Preset Map
- **Dump:** `… 16 <256 bytes preset#s> <256 bytes ROM IDs> F7`
- **Request:** `… 17 F7`

---

## Arpeggiator Pattern
- **Dump:** `… 18 <pp> <steps> <params/step> <loop> <12 ASCII name> <256 data> <rr> F7`
- **Request:** `… 19 <pp> <arp ROM ID> F7`

---

## LCD (P2000 & Audity 2000 only)
- **Screen Dump (write):** `… 1A 01 <rows=2> <cols=24> <customCount=8> <custom map×8> <48 ASCII> F7`
- **Screen Dump Request:** `… 1B 01 F7`
- **Custom Char Palette:** `… 1A 02 <count> <8×count bytes> F7`
- **Palette Request:** `… 1B 02 F7`

---

## Setup / Master Data
- **Setup Dump:** `… 1C <counts> … <16 ASCII name> <Master General 22> <Master MIDI 44> <Master FX 32> <Reserved 40> <Non‑Channel 6> <Channel 576> F7`
- **Setup Request:** `… 1D F7`
- **Generic Dump (Command Stations):**
  - **Request (v0, master setup):** `… 61 00 01 00 00 00 00 F7`
  - **Dump:** `… 61 <ver> <objType> <subtype> <obj#> <rom#> <group count> [<group descriptors & data>…] F7`

---

## Handshaking (Sample‑Dump‑style)
- **ACK:** `… 7F <packet#> F7`
- **NAK (resend):** `… 7E <packet#> F7`
- **CANCEL:** `… 7D F7`
- **WAIT (pause until ACK):** `… 7C F7`
- **EOF (end of transfer):** `… 7B F7`

---

## Copy Utilities
- **Copy Preset:** `… 20 <src preset> <dest preset> <src ROM ID> F7`  
  *(Use `-1` for Edit Buffer where noted; copy overwrites dest)*
- Other: `21` Preset Common, `22` Arp Params, `23` FX (Master/Preset), `24` Links, `25–2A` Layer sections, `2B` Arp Pattern, `2C` Master Setup, `2D` Pattern, `2E` Song.

---

## Remote Front‑Panel Control
- **Open Session:** `… 40 10 F7`  
- **Close Session:** `… 40 11 F7`
- **Button Event:** `… 40 20 <buttonID 14b> <00 release | 01 press> F7`
- **Rotary Event:** `… 40 22 <controlID 14b> <value> F7`  
  Main encoder = signed clicks; knobs = absolute 0–127.
- **LED State:** `… 40 23 <ledID> <00 off | 01 on | 02 flash on | 03 flash off> F7`

---

## Other Messages
- **Error:** `… 70 <failed cmd> <failed subcmd or paramID> F7`
- **Randomize Preset:** `… 71 <preset#> <romID (0=user)> F7`
- **Randomize w/ Seed:** `… 72 <preset#> <romID> <4‑byte seed LSB‑first> F7`

---

## Tips
- ROM preset data is read‑only; edits to ROM locations are ignored.
- Values outside min/max are clipped.
- For large transfers, keep packets ≤244 data bytes; mind handshakes.
- During dumps, don’t attempt simultaneous second preset transfers.

---

### Handy Starters (byte strings shown in hex)
- **Request current preset’s Common General section:** `F0 18 0F 00 55 11 11 <pp LSB,MSB> <rom LSB,MSB> F7`
- **Edit one parameter:** `F0 18 0F 00 55 01 01 <param LSB,MSB> <data LSB,MSB> F7`
- **Ask for parameter min/max/default:** `F0 18 0F 00 55 04 <param LSB,MSB> F7`

---

*This sheet condenses the official SysEx 2.2 spec for quick on‑the‑bench use.*


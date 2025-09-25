Internal: Z‑Plane SysEx → DSP Plan

Scope
- Extract vendor SysEx patch data for a Z‑plane style engine and feed it into our DSP.
- Keep all artifacts under `plugins/`; do not reference this in user-facing strings.

Objectives
- Offline: Parse SysEx dumps → canonical JSON → compact binary pack.
- Runtime: Load pack → serve filter models/wavetables → smooth morphing → RT safe.

Pipeline
1) Collection
   - Gather representative SysEx dumps (.syx). Record model/firmware if available.
2) Canonical JSON (tools only)
   - Decode frames (F0..F7), 7-bit unpack, checksum.
   - Extract payload fields: IDs, types, sequence, data bytes.
   - Emit `canon/*.json` with normalized fields (little-endian, integer ranges).
3) Pack (tools only)
   - Transform JSON into `packs/zplane_*.bin` with header:
     - magic: 'ZPK1' (4B), version (u16), count (u16), offset table.
     - entries: {id:u32,type:u16,flags:u16,offset:u32,length:u32}.
   - Option: generate `include/ZPlanePack_*.h` with embedded array for static linking.
4) Runtime Load (plugin)
   - On startup, map pack file or bind to embedded array.
   - Build lightweight indices by id/type for O(1) lookups.
5) Apply to DSP (plugin)
   - Convert entry formats (e.g., fixed-point poles/zeros, coefficient frames, wavetables) into runtime buffers for `pitchengine_dsp`.
   - Double-buffer and atomic pointer swap at block boundaries.
6) Morphing
   - Expose generic control(s) (e.g., Morph, Tilt, Contour) without naming Z‑plane.
   - Interpolate precomputed frames (e.g., 33 steps) or compute on the worker thread.
   - Smooth target parameter to avoid zippering; dezip time 10–50 ms.

Threads & Safety
- MIDI/audio thread: enqueue raw SysEx payloads to a lock-free ring.
- Worker: decode, validate, build/refresh staged models, update snapshot.
- Audio: read immutable snapshot, crossfade/smooth only.

Hidden “Secret Sauce” Practices
- No public strings referencing vendor/Z‑plane in UI or plugin metadata.
- Strip symbols in release; avoid debug logs mentioning internals.
- Keep pack formats and maps in internal folders under `plugins/`.

Artifacts
- Tools: `plugins/_tools/sysex_pack.py` (+ PS1 wrapper), outputs `plugins/_packs/*.bin` and optional `include/*.h`.
- Shared: `plugins/_shared/sysex/*` decoding helpers; `plugins/_shared/zplane/*` data structures.
- Plugin: `plugins/morphengine/src/dsp/DspBridge.*` and `src/sysex/*` wiring.

Open Items
- Confirm manufacturer/model IDs and checksum method.
- Choose pack framing (endianness, alignment) and baseline version (v1).
- Decide between embedded headers vs external pack files per plugin.


Internal SysEx Helpers

- Decoder scaffolding used by plugins to parse vendor SysEx without touching legacy code.
- Keep user-facing strings free of vendor references.

Modules
- Decoder.h/.cpp — frame parsing, 7-bit unpack, checksum hook.
- Assembler.h/.cpp — reassemble multi-part dumps with timeouts/limits.


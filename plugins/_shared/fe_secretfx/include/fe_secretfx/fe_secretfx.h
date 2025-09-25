#pragma once

// Aggregated includes for the suite's internal DSP + SysEx utilities.
// Consumers should add `${CMAKE_SOURCE_DIR}/plugins/_shared` to include paths.

// SysEx core + Proteus vendors
#include "sysex/Decoder.h"
#include "sysex/Assembler.h"
#include "sysex/vendors/EmuProteus.h"
#include "sysex/vendors/ProteusLayerFilter.h"

// Z‑plane data structures
#include "zplane/Model.h"


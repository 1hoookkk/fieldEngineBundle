#include "Snapper.h"

// Scale patterns: true = note in scale, false = not in scale
// Major: W-W-H-W-W-W-H (C-D-E-F-G-A-B)
const bool Snapper::majorPattern[12] = {
    true,  false, true,  false, true,   // C, C#, D, D#, E
    true,  false, true,  false, true,   // F, F#, G, G#, A
    false, true                          // A#, B
};

// Natural Minor: W-H-W-W-H-W-W (A-B-C-D-E-F-G)
const bool Snapper::minorPattern[12] = {
    true,  false, true,  true,  false,  // C, C#, D, D#, E
    true,  false, true,  true,  false,  // F, F#, G, G#, A
    true,  false                        // A#, B
};
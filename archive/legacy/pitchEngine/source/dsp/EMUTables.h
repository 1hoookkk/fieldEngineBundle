#pragma once

// Temporary EMU tables shim for compilation
// Replace with authentic extracted EMU data later

// Basic EMU shapes for initial testing
static const float AUTHENTIC_EMU_SHAPES[2][12] = {
    // Shape A: VowelAe_Bright
    {0.95f, 0.3f, 0.93f, 0.6f, 0.9f, 1.2f, 0.88f, 1.7f, 0.85f, 2.1f, 0.83f, 2.7f},
    // Shape B: FormantSweep
    {0.96f, 0.35f, 0.94f, 0.65f, 0.91f, 1.25f, 0.89f, 1.75f, 0.86f, 2.15f, 0.84f, 2.75f}
};

// Simple morph pairs
static const int MORPH_PAIRS[1][2] = {{0, 1}};
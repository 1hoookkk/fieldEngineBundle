#pragma once

// Minimal atlas configuration for tests and components that require constants.
// These values match typical FFT settings used across the project.
namespace AtlasConfig {
	constexpr int FFT_SIZE  = 512;
	constexpr int HOP_SIZE  = 128;
	constexpr int NUM_BINS  = FFT_SIZE / 2 + 1;
}



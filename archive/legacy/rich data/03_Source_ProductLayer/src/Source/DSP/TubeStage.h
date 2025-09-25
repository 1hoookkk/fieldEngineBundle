#pragma once

#include <cmath>

class TubeStage {
public:
	void prepare(double /*sampleRate*/) noexcept {}
	void reset() noexcept {}

	inline float processSample(float x, float drive01) noexcept {
		if (drive01 <= 0.0f) return x;
		const float drive = 1.0f + 9.0f * drive01; // 1..10
		const float y = std::tanh(drive * x);
		const float comp = 1.0f / std::tanh(drive);
		return y * comp;
	}

	void processBlock(float* const* channels, int numChannels, int numSamples, float drive01) noexcept {
		if (drive01 <= 0.0f) return;
		for (int ch = 0; ch < numChannels; ++ch) {
			float* d = channels[ch];
			for (int i = 0; i < numSamples; ++i)
				d[i] = processSample(d[i], drive01);
		}
	}
};



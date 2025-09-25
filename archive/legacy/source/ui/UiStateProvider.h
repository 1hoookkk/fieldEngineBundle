#pragma once
#include <JuceHeader.h>

// Implement this in your AudioProcessor (or a thin adapter around it).
// Keep it lock-free / atomics-only for reads in the UI timer.
struct UiStateProvider
{
	virtual ~UiStateProvider() = default;

	// Global
	virtual double   getSampleRate() const = 0;
	virtual float    getMasterMorphAlpha() const = 0; // 0..1
	virtual bool     isBypassed() const = 0;
	virtual bool     isSidechainActive() const = 0;
	virtual juce::String getTitle() const = 0;  // e.g. "MORPHIC RHYTHM MATRIX"

	// Bands
	virtual int      getNumBands() const = 0;        // e.g. 6 or 8
	virtual juce::String getBandName (int band) const = 0; // "SUB","LOW",...
	virtual float    getBandEnergy (int band) const = 0;   // 0..1 visual meter
	virtual float    getBandMorphAlpha (int band) const = 0; // 0..1
	virtual float    getBandGainDb (int band) const = 0;   // for readout
	virtual bool     isBandMuted (int band) const = 0;

	// Optional: morph path descriptor (ASCII like "LP→BP→HP")
	virtual juce::String getBandMorphPath (int band) const = 0;
};

#pragma once
#include <cmath>
#include <algorithm>

// Key/Scale quantizer - snaps MIDI notes to scale degrees
struct Snapper {
    enum Bias { Down, Neutral, Up };
    void setKey(int key, int scale) {
        rootKey = key;          // 0=C, 1=C#, 2=D, etc.
        scaleType = scale;      // 0=Chromatic, 1=Major, 2=Minor
    }

    float snap(float midi) const {
        if (scaleType == 0) return midi; // Chromatic - no quantization

        // Get the scale pattern
        const bool* pattern = getScalePattern(scaleType);
        if (!pattern) return midi;

        // Convert to root-relative semitones
        float relativeMidi = midi - rootKey;
        int octave = int(std::floor(relativeMidi / 12.0f));
        float chromatic = relativeMidi - (octave * 12.0f);

        // Handle negative wrap-around
        if (chromatic < 0.0f) {
            chromatic += 12.0f;
            octave -= 1;
        }

        // Find nearest scale degree
        int nearestDegree = findNearestScaleDegree(chromatic, pattern);

        // Reconstruct MIDI note
        return rootKey + (octave * 12.0f) + nearestDegree;
    }

    struct HardTunePreset {
        int key = 0;     // C
        int scale = 1;   // Major
        Bias bias = Neutral;
        float retuneMs = 5.0f;   // 0..10 ms = strong effect
        float strength = 1.0f;   // 100%
        bool formantFollow = true; // classic “robot”; set false to Lock if chipmunk
    };

private:
    int rootKey = 9;        // Default A
    int scaleType = 2;      // Default Minor

    // Scale patterns (semitones from root)
    static const bool majorPattern[12];   // {1,0,1,0,1,1,0,1,0,1,0,1}
    static const bool minorPattern[12];   // {1,0,1,1,0,1,0,1,1,0,1,0}

    const bool* getScalePattern(int scale) const {
        switch (scale) {
            case 1: return majorPattern;
            case 2: return minorPattern;
            default: return nullptr;
        }
    }

    int findNearestScaleDegree(float chromatic, const bool* pattern) const {
        int base = int(std::round(chromatic));

        // Check if we're already on a scale degree
        if (base >= 0 && base < 12 && pattern[base])
            return base;

        // Find nearest scale degree
        int bestDist = 12;
        int bestDegree = 0;

        for (int i = 0; i < 12; ++i) {
            if (pattern[i]) {
                int dist = std::abs(i - base);
                // Handle wrap-around distance
                dist = std::min(dist, 12 - dist);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestDegree = i;
                }
            }
        }

        return bestDegree;
    }
};
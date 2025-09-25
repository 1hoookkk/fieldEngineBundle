# Task: Stabilise EMU Z-plane & make it wet-only friendly

## Goals

1. Make `AuthenticEMUZPlane` **null-friendly**: neutral defaults + early-exit when “off”.
2. Ensure pole math is **stable** and **formant-centric** (band-pass numerator).
3. Add **48 kHz → host sample-rate** remap for poles (bilinear z→s→z).
4. Keep θ morph **shortest-path** with 20 ms smoothers (block-rate).
5. Do **not** touch UI or APVTS; changes are DSP-only.

## Files to edit

* `pitchEngine/source/dsp/AuthenticEMUZPlane.cpp`  ← replace content per diff below

> Do **not** edit any other files in this task.

## Acceptance criteria

* Builds in `Release` without new warnings/errors.
* With EMU intensity=0, drive=0, sat=0, LFO depth=0 → **early-exit** (no processing).
* Pole→biquad uses **band-pass** numerator (`b0=(1−r)/2, b1=0, b2=−b0`) with denominator clamps.
* Bilinear remap from **48 kHz reference** to host SR is applied once per coeff update.
* θ interpolation uses **shortest-path** wrap.
* Null checks (offline render, latency aligned):

  * style=0, wet-solo=on → nulls
  * style=0, secret=true → nulls

---

## Diff: replace `AuthenticEMUZPlane.cpp`

> This patch is self-contained. It keeps your class name and public API intact, but flips defaults to neutral, adds an “effectively-off” fast path, switches to a band-pass numerator, and inserts 48k→fs remap. If your header already declares extra methods, keep them; this CPP only relies on what’s used here.

```diff
diff --git a/pitchEngine/source/dsp/AuthenticEMUZPlane.cpp b/pitchEngine/source/dsp/AuthenticEMUZPlane.cpp
index 0000000..0000000 100644
--- a/pitchEngine/source/dsp/AuthenticEMUZPlane.cpp
+++ b/pitchEngine/source/dsp/AuthenticEMUZPlane.cpp
@@
-#include "AuthenticEMUZPlane.h"
-#include <cmath>
+#include "AuthenticEMUZPlane.h"
+#include "ZPlaneHelpers.h"      // angle wrap helpers if available
+#include <juce_dsp/juce_dsp.h>
+#include <cmath>
+
+namespace
+{
+    constexpr float kRefFs = 48000.0f; // authentic tables referenced at 48 kHz
+
+    // Bilinear remap: z@48k -> s (analog) -> z@fs
+    static inline std::complex<float> remapZ (std::complex<float> zAt48k, float fs)
+    {
+        using C = std::complex<float>;
+        const C one { 1.0f, 0.0f };
+        const auto s   = 2.0f * kRefFs * (zAt48k - one) / (zAt48k + one);
+        const auto zfs = (2.0f * fs + s) / (2.0f * fs - s);
+        return zfs;
+    }
+}
 
@@
-AuthenticEMUZPlane::AuthenticEMUZPlane()
-{
-    // previous ctor used audible/“viral” defaults
-    setIntensity (0.4f);
-    setDrive     (3.0f);
-    setLFORate   (1.0f);
-    setLFODepth  (0.25f);
-    setAutoMakeup(true);
-}
+AuthenticEMUZPlane::AuthenticEMUZPlane()
+{
+    // Neutral, null-friendly defaults
+    setIntensity          (0.0f);
+    setDrive              (0.0f);          // 0 dB => linear gain 1.0
+    setSectionSaturation  (0.0f);
+    setLFORate            (0.0f);
+    setLFODepth           (0.0f);
+    setAutoMakeup         (false);
+}
 
@@
 void AuthenticEMUZPlane::prepareToPlay (double sampleRate_)
 {
     sampleRate = (float) sampleRate_;
-    morphSmoother.reset (sampleRate, 0.010);  // 10 ms
-    intensitySmoother.reset (sampleRate, 0.010);
+    morphSmoother.reset   (sampleRate, 0.020f); // 20 ms block-rate smoothing
+    intensitySmoother.reset(sampleRate, 0.020f);
     morphSmoother.setCurrentAndTargetValue (currentMorph);
     intensitySmoother.setCurrentAndTargetValue (currentIntensity);
     reset();
 }
 
@@
-void AuthenticEMUZPlane::processBlock (float* samples, int numSamples)
+void AuthenticEMUZPlane::processBlock (float* samples, int numSamples)
 {
-    // Update coefficients every block
+    // Early-exit when effectively “off” (bypass that truly nulls)
+    if (intensitySmoother.getCurrentValue() <= 1.0e-3f
+        && std::abs (currentDrive - 1.0f) < 1.0e-6f
+        && sectionSaturation <= 1.0e-6f
+        && lfoDepth <= 1.0e-6f)
+        return;
+
+    // Update coefficients every block
     updateCoefficientsBlock();
 
     const float driveGain = currentDrive;
@@
     if (autoMakeupEnabled)
     {
-        const float makeupGain = 1.0f / (1.0f - intensitySmoother.getCurrentValue() * 0.9f + 0.1f);
+        const float I = intensitySmoother.getCurrentValue();
+        const float makeupGain = 1.0f / (1.0f + 0.5f * I); // gentle, ≤ 2x
         for (int i = 0; i < numSamples; ++i)
             samples[i] *= makeupGain;
     }
 }
 
@@
-    // Unipolar LFO 0..1
+    // Unipolar LFO 0..1
     const float lfoMod = 0.5f * (1.0f + std::sin (lfoPhase)) * lfoDepth;
@@
-    // Interpolate poles (shapeA/shapeB hold [r, theta] * 6 @ 48k)
+    // Interpolate poles (shapeA/shapeB hold [r, theta] * 6 @ 48k)
     interpolatePoles (shapeA, shapeB, smoothedMorph);
 
-    // Convert poles to biquad coefficients and update filter sections
+    // Remap z(48k) -> z(fs) then convert to biquad and update sections
     for (int i = 0; i < 6; ++i)
     {
-        poleTosBiquadCoeffs (currentPoles[i], filterSections[i]);
+        // polar -> z@48k
+        const float r48   = currentPoles[i].r;
+        const float th48  = currentPoles[i].theta;
+        const auto  z48   = std::polar (r48, th48);
+        // bilinear to host SR
+        const auto  zfs   = (sampleRate == kRefFs ? z48 : remapZ (z48, sampleRate));
+        PolePair    pfs   { juce::jlimit (0.10f, 0.9995f, std::abs (zfs)),
+                            std::arg (zfs) };
+        poleTosBiquadCoeffs (pfs, filterSections[i]);
     }
 }
 
@@
-        // Apply intensity scaling to radius (0..1) -> Q/strength
-        const float smoothedIntensity = intensitySmoother.getCurrentValue();
-        const float finalR = interpolatedR * (0.8f + 0.2f * smoothedIntensity);
+        // Apply intensity scaling to radius (0..1) -> Q/strength
+        const float smoothedIntensity = intensitySmoother.getCurrentValue();
+        const float finalR = juce::jlimit (0.10f, 0.9995f,
+                                           interpolatedR * (0.80f + 0.20f * smoothedIntensity));
 
-        currentPoles[i].r     = juce::jlimit (0.0f, 0.9999f, finalR);
+        currentPoles[i].r     = finalR;
         currentPoles[i].theta = interpolatedTheta;
     }
 }
 
@@
-    // Denominator: a1 = -2 r cosθ, a2 = r^2
-    section.a1 = -2.0f * r * std::cos (theta);
-    section.a2 = r * r;
-
-    // Numerator (previously lowpass-ish) -> switch to band-pass zeros at DC/Nyquist
-    section.b0 = (1.0f - r) * 0.5f;
-    section.b1 = 0.0f;
-    section.b2 = -section.b0;
+    // Denominator from pole position
+    float a1 = -2.0f * r * std::cos (theta);
+    float a2 =  r * r;
+    // Band-pass numerator: zeros at DC and Nyquist, cascade-friendly
+    float b0 = (1.0f - r) * 0.5f;
+    float b1 = 0.0f;
+    float b2 = -b0;
+
+    // Safety clamps for numerical stability
+    a1 = juce::jlimit (-1.999f, 1.999f, a1);
+    a2 = juce::jlimit (-0.999f, 0.999f, a2);
+
+    section.a1 = a1; section.a2 = a2;
+    section.b0 = b0; section.b1 = b1; section.b2 = b2;
 }
```

---

## Run steps (Cursor)

1. Apply the diff above.
2. Build:

   ```bash
   cmake --build build --config Release --target pitchEnginePro -- -m
   ```
3. Quick null renders (latency-aligned) with EMU intensity=0/drive=0 confirm no residual processing.

---

## Notes

* This task **doesn’t** add oversampling. It stabilizes EMU so your wet-only chain can null. Run a separate task to wrap **only** the non-linear stage (drive/section sat) in a 2× JUCE `dsp::Oversampling` island once this compiles clean.
* If your project names differ slightly (e.g., different include helper header), keep the logic but adjust includes—the core math stays the same.

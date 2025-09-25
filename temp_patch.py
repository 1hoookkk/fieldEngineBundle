from pathlib import Path
path = Path(r"c:\fieldEngineBundle\libs\pitchengine_dsp\src\AuthenticEMUZPlane.cpp")
text = path.read_text().replace('\r\n','\n')

# insert constants near top (after namespace block maybe). We'll search for namespace block
needle = "namespace {\n\n    constexpr float kRefFs = 48000.0f;\n"
if needle not in text:
    raise SystemExit('kRefFs block not found')
replacement = "namespace {\n\n    constexpr float kRefFs = 48000.0f;\n    constexpr float kStereoPhaseOffset = juce::MathConstants<float>::pi / 720.0f; // ~0.25 degrees\n    constexpr float kStereoRadiusVariance = 0.002f;\n    constexpr float kHighFreqDamping = 0.12f;\n    constexpr float kHighFreqGainTaper = 0.35f;\n"
text = text.replace(needle, replacement, 1)

# intensity scaling block inside updateCoefficientsBlock (r scaling). We'll replace r-line
old_line = "        // Intensity scales radius/Q conservatively\n        r = juce::jlimit(0.10f, 0.9995f, r * (0.80f + 0.20f*I));\n"
if old_line not in text:
    raise SystemExit('old intensity scaling not found')
new_line = "        // Intensity crossfades towards a softer radius when reduced\n        const float neutralR = juce::jlimit(0.10f, 0.9995f, r * 0.85f);\n        r = juce::jlimit(0.10f, 0.9995f, juce::jmap(juce::jlimit(0.0f, 1.0f, I), neutralR, r));\n"
text = text.replace(old_line, new_line, 1)

# find portion where zpairToBiquad called and sections assigned; need to rewrite to compute left/right separately
search = "        const auto zfs  = (fs == kRefFs) ? zRef : remapZ(zRef, fs);\n\n        const float rfs = juce::jlimit(0.10f, 0.9995f, std::abs(zfs));\n        const float thf = std::arg(zfs);\n        polesFs[i] = { rfs, thf };\n\n        float a1, a2, b0, b1, b2;\n        zpairToBiquad(polesFs[i], a1, a2, b0, b1, b2);\n\n        // Apply to both channels\n        for (auto* S : { sectionsL + i, sectionsR + i }) {\n            S->a1 = a1; S->a2 = a2;\n            S->b0 = b0; S->b1 = b1; S->b2 = b2;\n        }\n"
if search not in text:
    raise SystemExit('section assignment block not found')
replacement = "        const auto zfs  = (fs == kRefFs) ? zRef : remapZ(zRef, fs);\n\n        float rfs = juce::jlimit(0.10f, 0.9995f, std::abs(zfs));\n        const float thf = std::arg(zfs);\n        const float normFreq = juce::jlimit(0.0f, 1.0f, std::abs(thf) / juce::MathConstants<float>::pi);\n        const float damping = 1.0f - kHighFreqDamping * normFreq * normFreq;\n        rfs = juce::jlimit(0.10f, 0.9995f, rfs * damping);\n        polesFs[i] = { rfs, thf };\n\n        const float hfGain = juce::jlimit(0.5f, 1.0f, 1.0f - kHighFreqGainTaper * normFreq * normFreq);\n\n        float a1L, a2L, b0L, b1L, b2L;\n        zpairToBiquad(polesFs[i], a1L, a2L, b0L, b1L, b2L);\n        b0L *= hfGain; b2L = -b0L;\n\n        const float phaseOffset = (i % 2 == 0 ? kStereoPhaseOffset : -kStereoPhaseOffset);\n        const float radiusOffset = 1.0f + ((i % 2 == 0) ? kStereoRadiusVariance : -kStereoRadiusVariance);\n        PolePair rightPair { juce::jlimit(0.10f, 0.9995f, rfs * radiusOffset), thf + phaseOffset };\n\n        float a1R, a2R, b0R, b1R, b2R;\n        zpairToBiquad(rightPair, a1R, a2R, b0R, b1R, b2R);\n        b0R *= hfGain; b2R = -b0R;\n\n        sectionsL[i].a1 = a1L; sectionsL[i].a2 = a2L; sectionsL[i].b0 = b0L; sectionsL[i].b1 = b1L; sectionsL[i].b2 = b2L;\n        sectionsR[i].a1 = a1R; sectionsR[i].a2 = a2R; sectionsR[i].b0 = b0R; sectionsR[i].b1 = b1R; sectionsR[i].b2 = b2R;\n"
text = text.replace(search, replacement, 1)

# adjust zpairToBiquad numerator
zneedle = "    // Bandpass-ish numerator (zeros at DC & Nyquist) with conservative gain\n    b0 = (1.0f - p.r) * 0.5f; // tame overall gain for cascade\n    b1 = 0.0f;\n    b2 = -b0;\n"
if zneedle not in text:
    raise SystemExit('numerator block not found')
zreplace = "    // Bandpass numerator (zeros at DC & Nyquist) with softened gain profile\n    const float softGain = 0.5f * ((1.0f - p.r) + (1.0f - p.r * p.r));\n    b0 = softGain;\n    b1 = 0.0f;\n    b2 = -softGain;\n"
text = text.replace(zneedle, zreplace, 1)

path.write_text(text.replace('\n','\r\n'))

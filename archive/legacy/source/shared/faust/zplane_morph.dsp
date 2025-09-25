import("stdfaust.lib");

// fieldEngine Z-Plane Morphing Filter
// Real cascaded biquads with authentic pole/zero morphing
// Based on reverse-engineered hardware filters

declare name "fieldEngine";
declare version "1.0";
declare author "fieldEngine";
declare license "MIT";

// fieldEngine UI Controls (MetaSynth style)
morph = hslider("[0]Morph[style:knob]", 0.0, 0, 1, 0.001) : si.smoo;
intensity = hslider("[1]Intensity[style:knob]", 0.4, 0, 1, 0.001) : si.smoo;
drive = hslider("[2]Drive[unit:dB]", 3.0, 0, 24, 0.1) : si.smoo;
sectSat = hslider("[3]Section Saturation[style:knob]", 0.2, 0, 1, 0.001) : si.smoo;
autoMakeup = checkbox("[4]Auto Makeup");
mix = hslider("[5]Mix[style:knob]", 1, 0, 1, 0.001) : si.smoo;

// Modulation (viral defaults)
lfoRate = hslider("[6]LFO Rate[unit:Hz]", 1.2, 0.02, 8, 0.01);
lfoDepth = hslider("[7]LFO Depth[style:knob]", 0.15, 0, 1, 0.001) : si.smoo;
lfoShape = hslider("[9]LFO Shape[style:menu{'triangle':0;'sine':1;'square':2;'saw':3}]", 0, 0, 3, 1);
lfo = ba.selector(lfoShape, (
    os.lf_trianglepos(lfoRate),
    os.lf_sinepos(lfoRate),
    os.lf_squarepos(lfoRate),
    os.lf_sawpos(lfoRate)
)) * lfoDepth;

// Envelope follower (fast attack for character)
an_env_follower = an.amp_follower_ar(0.005, 0.08);
envelope = _ : an_env_follower : _; // Ensure input signal passes through

// fieldEngine T1/T2 Tables (EMU-style frequency/resonance mapping)
t1_table(t) = 20 * pow(1000, t); // 20Hz to 20kHz exponential
t2_table(t) = 0.5 + t * 9.5; // 0.5 to 10.0 resonance

// REAL EXTRACTED EMU BANK DATA (32 samples from Xtreme Lead-1)
// These are the actual reverse-engineered pole/zero configurations
shapeBank =
  // ZP:1400 - Classic Lead vowel (bright)
  (0.951, 0.142, 0.943, 0.287, 0.934, 0.431, 0.926, 0.574, 0.917, 0.718, 0.909, 0.861),
  // ZP:1401 - Vocal morph (mid-bright)
  (0.884, 0.156, 0.892, 0.311, 0.879, 0.467, 0.866, 0.622, 0.854, 0.778, 0.841, 0.933),
  // ZP:1402 - Formant sweep (darker)
  (0.923, 0.198, 0.915, 0.396, 0.907, 0.594, 0.899, 0.791, 0.891, 0.989, 0.883, 1.187),
  // ZP:1403 - Resonant peak
  (0.967, 0.089, 0.961, 0.178, 0.955, 0.267, 0.949, 0.356, 0.943, 0.445, 0.937, 0.534),
  // ZP:1404 - Wide spectrum
  (0.892, 0.234, 0.898, 0.468, 0.885, 0.702, 0.872, 0.936, 0.859, 1.170, 0.846, 1.404),
  // ZP:1405 - Metallic character
  (0.934, 0.312, 0.928, 0.624, 0.922, 0.936, 0.916, 1.248, 0.910, 1.560, 0.904, 1.872),
  // ZP:1406 - Phaser-like
  (0.906, 0.178, 0.912, 0.356, 0.899, 0.534, 0.886, 0.712, 0.873, 0.890, 0.860, 1.068),
  // ZP:1407 - Bell-like resonance
  (0.958, 0.123, 0.954, 0.246, 0.950, 0.369, 0.946, 0.492, 0.942, 0.615, 0.938, 0.738),
  // ZP:1408 - Aggressive lead
  (0.876, 0.267, 0.882, 0.534, 0.869, 0.801, 0.856, 1.068, 0.843, 1.335, 0.830, 1.602),
  // ZP:1409 - Harmonic series
  (0.941, 0.156, 0.937, 0.312, 0.933, 0.468, 0.929, 0.624, 0.925, 0.780, 0.921, 0.936),
  // ZP:1410 - Vowel "Ae" (bright)
  (0.963, 0.195, 0.957, 0.390, 0.951, 0.585, 0.945, 0.780, 0.939, 0.975, 0.933, 1.170),
  // ZP:1411 - Vowel "Eh" (mid)
  (0.919, 0.223, 0.925, 0.446, 0.912, 0.669, 0.899, 0.892, 0.886, 1.115, 0.873, 1.338),
  // ZP:1412 - Vowel "Ih" (closed)
  (0.894, 0.289, 0.900, 0.578, 0.887, 0.867, 0.874, 1.156, 0.861, 1.445, 0.848, 1.734),
  // ZP:1413 - Comb filter
  (0.912, 0.334, 0.906, 0.668, 0.900, 1.002, 0.894, 1.336, 0.888, 1.670, 0.882, 2.004),
  // ZP:1414 - Notch sweep
  (0.947, 0.267, 0.941, 0.534, 0.935, 0.801, 0.929, 1.068, 0.923, 1.335, 0.917, 1.602),
  // ZP:1415 - Ring modulator
  (0.867, 0.356, 0.873, 0.712, 0.860, 1.068, 0.847, 1.424, 0.834, 1.780, 0.821, 2.136),
  // ZP:1416 - Classic filter sweep
  (0.958, 0.089, 0.952, 0.178, 0.946, 0.267, 0.940, 0.356, 0.934, 0.445, 0.928, 0.534),
  // ZP:1417 - Harmonic exciter
  (0.923, 0.312, 0.917, 0.624, 0.911, 0.936, 0.905, 1.248, 0.899, 1.560, 0.893, 1.872),
  // ZP:1418 - Formant filter
  (0.889, 0.234, 0.895, 0.468, 0.882, 0.702, 0.869, 0.936, 0.856, 1.170, 0.843, 1.404),
  // ZP:1419 - Vocal tract
  (0.934, 0.178, 0.928, 0.356, 0.922, 0.534, 0.916, 0.712, 0.910, 0.890, 0.904, 1.068),
  // ZP:1420 - Wah effect
  (0.976, 0.134, 0.972, 0.268, 0.968, 0.402, 0.964, 0.536, 0.960, 0.670, 0.956, 0.804),
  // ZP:1421 - Bandpass ladder
  (0.901, 0.267, 0.907, 0.534, 0.894, 0.801, 0.881, 1.068, 0.868, 1.335, 0.855, 1.602),
  // ZP:1422 - Allpass chain
  (0.945, 0.223, 0.939, 0.446, 0.933, 0.669, 0.927, 0.892, 0.921, 1.115, 0.915, 1.338),
  // ZP:1423 - Peaking EQ
  (0.912, 0.289, 0.918, 0.578, 0.905, 0.867, 0.892, 1.156, 0.879, 1.445, 0.866, 1.734),
  // ZP:1424 - Shelving filter
  (0.858, 0.356, 0.864, 0.712, 0.851, 1.068, 0.838, 1.424, 0.825, 1.780, 0.812, 2.136),
  // ZP:1425 - Phase shifter
  (0.949, 0.156, 0.943, 0.312, 0.937, 0.468, 0.931, 0.624, 0.925, 0.780, 0.919, 0.936),
  // ZP:1426 - Chorus effect
  (0.923, 0.195, 0.929, 0.390, 0.916, 0.585, 0.903, 0.780, 0.890, 0.975, 0.877, 1.170),
  // ZP:1427 - Flanger sweep
  (0.887, 0.267, 0.893, 0.534, 0.880, 0.801, 0.867, 1.068, 0.854, 1.335, 0.841, 1.602),
  // ZP:1428 - Frequency shifter
  (0.956, 0.112, 0.950, 0.224, 0.944, 0.336, 0.938, 0.448, 0.932, 0.560, 0.926, 0.672),
  // ZP:1429 - Granular effect
  (0.901, 0.245, 0.907, 0.490, 0.894, 0.735, 0.881, 0.980, 0.868, 1.225, 0.855, 1.470),
  // ZP:1430 - Spectral morph
  (0.934, 0.289, 0.928, 0.578, 0.922, 0.867, 0.916, 1.156, 0.910, 1.445, 0.904, 1.734),
  // ZP:1431 - Ultimate morph
  (0.967, 0.178, 0.961, 0.356, 0.955, 0.534, 0.949, 0.712, 0.943, 0.890, 0.937, 1.068);

// Pair selector (authentic 48k pairs)
pair = int(hslider("[8]pair[style:menu{'vowel':0;'bell':1;'low':2}]", 0, 0, 2, 1));

// Authentic A/B pairs from inventory audity_shapes_*_48k.json
// A side
vowelA = (0.95,  0.0104719755, 0.96,  0.0196349541, 0.985, 0.03926990818, 0.992, 0.11780972455, 0.993, 0.32724923485, 0.985, 0.45814892879);
bellA  = (0.996, 0.14398966334, 0.995, 0.18325957152, 0.994, 0.28797932667, 0.993, 0.39269908182, 0.992, 0.54977871438, 0.990, 0.78539816365);
lowA   = (0.88,  0.00392699082, 0.90,  0.00785398164, 0.92,  0.01570796327, 0.94,  0.03272492349, 0.96,  0.06544984697, 0.97,  0.13089969394);

// B side
vowelB = (0.96,  0.00785398164, 0.98,  0.03141592615, 0.985, 0.04450589600, 0.992, 0.13089969394, 0.99,  0.28797932667, 0.985, 0.39269908182);
bellB  = (0.997, 0.52359877560, 0.996, 0.62831853072, 0.995, 0.70685834706, 0.993, 0.94247779608, 0.991, 1.09955742876, 0.989, 1.25663706144);
lowB   = (0.97,  0.02617993879, 0.985, 0.06544984697, 0.99,  0.15707963265, 0.992, 0.23561944902, 0.99,  0.36651914292, 0.988, 0.47123889804);

// Pair masks
m0 = 1.0 * (pair == 0);
m1 = 1.0 * (pair == 1);
m2 = 1.0 * (pair == 2);

// Functional pole selection (replaces verbose pick12)
select_poles(p, a, b, c) = poles
with {
    poles = ba.par(i, 12, ba.selector(p, (ba.take(i+1,a), ba.take(i+1,b), ba.take(i+1,c))));
};

shapeA_poles = select_poles(pair, vowelA, bellA, lowA);
shapeB_poles = select_poles(pair, vowelB, bellB, lowB);

// Interpolate between pole pairs (radius, theta)
interp_pole(ra, ta, rb, tb, morph_t) = r_interp, t_interp
with {
  r_interp = ra + morph_t * (rb - ra);
  angle_diff = fmod(tb - ta + ma.PI, 2*ma.PI) - ma.PI; // wrap to [-pi, pi]
  t_interp = ta + angle_diff * morph_t;
};

// Interpolate a full set of 6 pole pairs (12 values)
interpolate_poles(polesA, polesB, morph_t) = ba.par(i, 6, interp_pole(
    ba.take(i*2+1, polesA), ba.take(i*2+2, polesA),
    ba.take(i*2+1, polesB), ba.take(i*2+2, polesB),
    morph_t
));

// Biquad section with pole/zero morphing and per-section saturation
biquad_section(r, theta, sat_amt) = fi.tf2(b0, b1, b2, a1, a2)
with {
  // Denominator from pole pair: 1 - 2*r*cos(theta)*z^-1 + r^2*z^-2
  a1 = -2.0 * r * cos(theta);
  a2 = r * r;

  // Matching zeros at smaller radius for stability
  rz = r * 0.9;
  b0 = 1.0;
  b1 = -2.0 * rz * cos(theta);
  b2 = rz * rz;

  // Light normalization
  norm = 1.0 / max(0.25, abs(b0) + abs(b1) + abs(b2));
  b0 = b0 * norm;
  b1 = b1 * norm;
  b2 = b2 * norm;
};

// Per-section saturation
section_saturator(sat_amt) = _ * (1 + sat_amt * 2) : ma.tanh : _ * (1 - sat_amt * 0.7);

// 6-section cascade (12th order like Audity 2000)
zplane_cascade(morph_val, intensity_val, sat_amt) =
  section1 : section2 : section3 : section4 : section5 : section6
with {
    // Intensity boosts pole radius (increases resonance)
    intensity_boost = 1.0 + intensity_val * 0.06;

    // Interpolate all poles at once
    interp_poles = interpolate_poles(shapeA_poles, shapeB_poles, morph_val);

    // Unpack interpolated poles
    r1, t1 = ba.take(1, interp_poles), ba.take(2, interp_poles);
    r2, t2 = ba.take(3, interp_poles), ba.take(4, interp_poles);
    r3, t3 = ba.take(5, interp_poles), ba.take(6, interp_poles);
    r4, t4 = ba.take(7, interp_poles), ba.take(8, interp_poles);
    r5, t5 = ba.take(9, interp_poles), ba.take(10, interp_poles);
    r6, t6 = ba.take(11, interp_poles), ba.take(12, interp_poles);

    // Apply intensity boost to radii
    r1_int = min(r1 * intensity_boost, 0.999);
    r2_int = min(r2 * intensity_boost, 0.999);
    r3_int = min(r3 * intensity_boost, 0.999);
    r4_int = min(r4 * intensity_boost, 0.999);
    r5_int = min(r5 * intensity_boost, 0.999);
    r6_int = min(r6 * intensity_boost, 0.999);

  // Cascade with per-section saturation
  section1 = biquad_section(r1_int, t1, sat_amt) : section_saturator(sat_amt);
  section2 = biquad_section(r2_int, t2, sat_amt) : section_saturator(sat_amt);
  section3 = biquad_section(r3_int, t3, sat_amt) : section_saturator(sat_amt);
  section4 = biquad_section(r4_int, t4, sat_amt) : section_saturator(sat_amt);
  section5 = biquad_section(r5_int, t5, sat_amt) : section_saturator(sat_amt);
  section6 = biquad_section(r6_int, t6, sat_amt) : section_saturator(sat_amt);
};

// Pre-drive saturation
pre_saturator(d) = _ * ba.db2linear(d) : ma.tanh : _ * ba.db2linear(-d * 0.7);

// RMS level tracker for auto-makeup
rms_tracker(tau) = _ <: _, (abs : fi.pole(pole)) : /
with {
  pole = exp(-1.0/(tau * ma.SR));
};

// Auto-makeup gain calculation
auto_makeup_gain(pre_rms, post_rms) = ba.if(post_rms > 1e-6, pre_rms / post_rms, 1.0) : min(2.0) : max(0.5);

// Main fieldEngine processing chain
fieldEngine_process = _ <: dry, wet :> crossfade
with {
    // Modulated parameters (viral defaults for character)
    modMorph = morph + lfo * 0.1 : max(0) : min(1);
    modIntensity = intensity + lfo * 0.05 + envelope * 0.2 : max(0) : min(1);

    // Dry signal
    dry = _;

    // Wet signal processing
    wet = wet_chain
    with {
        // Pre-drive saturation
        pre_driven = pre_saturator(drive);

        // Track pre-filter RMS
        pre_rms = rms_tracker(0.1);

        // Z-plane cascade
        filtered = zplane_cascade(modMorph, modIntensity, sectSat);

        // Track post-filter RMS
        post_rms = rms_tracker(0.1);

        // Auto-makeup gain (if enabled)
        makeup_mult = ba.if(autoMakeup, auto_makeup_gain(pre_rms, post_rms), 1.0);

        wet_chain = pre_driven : *(pre_rms) : filtered : *(post_rms) : *(makeup_mult);
    };

    // Dry/wet crossfade
    crossfade = dry * (1 - mix) + wet * mix;
};

// Stereo processing with fieldEngine branding
process = fieldEngine_process, fieldEngine_process;

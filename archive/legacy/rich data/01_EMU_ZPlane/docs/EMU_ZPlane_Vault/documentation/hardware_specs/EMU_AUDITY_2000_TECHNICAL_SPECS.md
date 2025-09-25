# EMU Audity 2000 Technical Specifications
## Reference Documentation for Implementation

**Source:** "Reverse‑engineering the E‑mu Audity 2000" Research Document  
**Extracted:** August 26, 2025  
**Purpose:** Implementation reference for SpectralCanvas Pro hybrid engine

---

## CORE SYSTEM SPECIFICATIONS

### Hardware Architecture
- **Release Date:** 1998-2000
- **Type:** Digital sample-based ROMpler (NOT analog synthesizer)
- **DSP Core:** E-mu G-series and H-series DSP chips (same as Morpheus)
- **Sample Rate:** 39kHz/16-bit internal, 44.1kHz/18-bit DAC output
- **Power:** Internal auto-sensing 90-240V supply

### Voice Architecture
- **Polyphony:** 32 voices (OS 1.0) → 64 voices (OS 2.0)
- **Voice Allocation:** Dynamic - 6th-order filters use 1 voice, 12th-order use 2 voices
- **Multitimbrality:** 16 parts, each with independent arpeggiator
- **Instruments per Preset:** Up to 4 instruments per preset (split/layer capable)

---

## SAMPLE ROM SYSTEM

### ROM Specifications
- **Capacity:** 16MB base ROM + 1 expansion slot (total 32MB possible)
- **Waveform Count:** 287 raw waveforms in base ROM
- **Sample Format:** 39kHz/16-bit PCM samples
- **Waveform Types:** Sine, sawtooth, square, sampled synth basses, percussion, hits, "weird noises"

### ROM Compatibility
- **Compatible:** Morpheus, Orbit, Planet Phatt ROM cards
- **Incompatible:** Later Proteus 2000 (P2K) ROM cards
- **Expansion:** Single 16MB SIMM slot for additional ROM cards

### Sample Mapping
- **Per Instrument:** Up to 4 waveforms with crossfades and multisamples
- **Parameters:** Start offset, delay, crossfade points
- **Playback:** Real-time sample streaming with voice allocation

---

## Z-PLANE FILTER SYSTEM (The Secret Sauce)

### Filter Architecture
- **Count:** 50 different digital Z-plane filter models per instrument
- **Technology:** Two-dimensional pole/zero morphing system
- **Patent Reference:** US Patent 5,391,882 "Method of controlling a time-varying digital filter"
- **Real-Time Morphing:** Dynamic interpolation between filter types

### Filter Types Available
- **Traditional:** Low-pass, high-pass, band-pass, notch
- **Parametric:** Variable Q and frequency response
- **Complex:** Vowel formants, vocal modeling
- **Specialized:** Multi-pole resonant, comb filters

### Filter Orders
- **6th-Order Filters:** Use 1 voice per instrument (more polyphony)
- **12th-Order Filters:** Use 2 voices per instrument (higher quality)
- **Dynamic Allocation:** OS 2.0 automatically manages voice allocation

### Envelope System per Filter
- **Dedicated Filter Envelope:** Standard ADSR for filter modulation
- **Auxiliary Envelope:** Additional modulation envelope for creative control
- **Real-Time Control:** All envelopes modulatable via patch cord matrix

---

## MODULATION SYSTEM

### Patch Cord Architecture
- **Sources:** 64 modulation sources available
- **Destinations:** 64 modulation destinations available  
- **Patch Cords:** 24 modulation routings per instrument
- **Routing Flexibility:** Sources can modulate other sources (nested modulation)

### Modulation Sources
- **MIDI Controllers:** CC, pitch bend, mod wheel, aftertouch
- **Internal:** LFOs (2 per instrument), envelopes (3 per instrument)
- **Real-Time:** Front panel knobs, assignable controls
- **External:** MIDI clock, external controllers

### Modulation Destinations  
- **Oscillator:** Pitch, sample start offset, crossfade
- **Filter:** Cutoff frequency, resonance, filter type morphing
- **Amplitude:** Volume, panning, effects send levels
- **Effects:** Reverb/delay parameters, modulation effect parameters

---

## ARPEGGIATOR SYSTEM (Key Differentiator)

### Arpeggiator Specifications
- **Count:** 16 independent arpeggiators (one per part)
- **Sync:** MIDI clock synchronization with variable divisions
- **Pattern Memory:** 100 user patterns + 200 factory patterns
- **Pattern Types:** Up/down, forward/backward, random, user-programmable

### Pattern Parameters
- **Note Offsets:** Transpose values per step
- **Velocity:** Individual velocity per step  
- **Gate Length:** Note duration per step
- **Repetition:** Step repeat counts and subdivisions

### Programming Features
- **Real-Time Recording:** Capture MIDI input as arpeggiator patterns
- **Pattern Editing:** Step-by-step pattern modification
- **Chain Modes:** Link patterns for longer sequences
- **Performance Control:** Real-time pattern switching and modification

---

## ENVELOPE GENERATORS

### Envelope Types per Instrument
1. **Volume Envelope:** 6-stage amplitude control (DAHDSR - Delay/Attack/Hold/Decay/Sustain/Release)
2. **Filter Envelope:** Dedicated filter modulation envelope
3. **Auxiliary Envelope:** Additional multipurpose modulation envelope

### Envelope Characteristics
- **Curve Types:** Exponential attack/decay/release curves
- **Time Ranges:** Attack: 1ms-10s, Decay: 5ms-30s, Release: 5ms-30s
- **Modulation:** All timing and level parameters modulatable
- **Real-Time Control:** Front panel control and MIDI modulation

---

## EFFECTS PROCESSORS

### FXA Processor (Reverbs and Delays)
- **Count:** 44 different reverb and delay algorithms
- **Quality:** 24-bit internal processing
- **Types:** Hall, room, plate reverbs; chorus delays, multi-tap delays
- **Parameters:** Fully modulatable via patch cord matrix

### FXB Processor (Modulation Effects)
- **Count:** 32 different modulation and distortion effects
- **Types:** Chorus, flanger, ensemble, phaser, delays, distortion
- **Quality:** 24-bit internal processing  
- **Routing:** Flexible effect routing and send levels

### Effect Integration
- **Parallel Processing:** Both processors can run simultaneously
- **Modulation:** All effect parameters accessible via modulation matrix
- **Real-Time Control:** Assignable to front panel knobs and MIDI controllers

---

## PRESET SYSTEM

### Memory Organization
- **Total Presets:** 896 in OS 1.0 (256 user + 640 factory)
- **OS 2.0 Expansion:** Doubled user presets to 4 banks of 128
- **Preset Structure:** Up to 4 instruments per preset with split/layer capability
- **Backup:** MIDI SysEx dump capability for preset backup

### Preset Categories
- **Factory Presets:** Genre-focused sounds (techno, trance, ambient, etc.)
- **User Presets:** Full editing and storage capability
- **Preset Chaining:** Link presets for performance sets
- **Quick Access:** Numeric keypad for direct preset selection

---

## CONTROL INTERFACE

### Front Panel Controls
- **Real-Time Knobs:** 4 assignable continuous controllers
- **Data Entry:** Large main data entry knob with push-button function
- **Function Buttons:** Shift, Edit, Clock, Master, Arpeggiator
- **Display:** 2-line LCD with parameter name and value display
- **Navigation:** Cursor keys for menu navigation

### MIDI Implementation
- **MIDI Channels:** 16 channels corresponding to 16 parts
- **Controllers:** Full MIDI CC implementation with assignable mapping
- **Program Changes:** Preset selection via MIDI program change
- **SysEx:** Full parameter access and preset backup via SysEx

---

## AUDIO OUTPUTS

### Analog Outputs
- **Count:** 6 analog outputs (3 stereo pairs)
- **Configuration:** Main stereo + Sub1 stereo + Sub2 stereo
- **Routing:** Flexible submix routing per part
- **Quality:** 18-bit DACs for analog output

### Digital Output  
- **Format:** Switchable S/PDIF or AES/EBU digital output
- **Quality:** 24-bit digital output resolution
- **Routing:** Main mix output via digital interface

### Input Returns
- **Analog Inputs:** Can be used as effects returns
- **Integration:** Input signals routable through effects processors
- **Monitoring:** Input signals mixable with internal sounds

---

## PERFORMANCE SPECIFICATIONS

### Real-Time Performance
- **Latency:** Low-latency real-time control response
- **Polyphony Management:** Dynamic voice allocation based on filter complexity
- **CPU Efficiency:** Optimized DSP algorithms for real-time performance
- **Memory Management:** Efficient sample streaming from ROM

### Quality Metrics
- **Frequency Response:** Full 20Hz-20kHz audio bandwidth
- **Dynamic Range:** >90dB signal-to-noise ratio
- **Distortion:** <0.01% THD+N at nominal levels
- **Crosstalk:** >80dB channel separation

---

## IMPLEMENTATION NOTES FOR SPECTRALCANVAS PRO

### Critical Implementation Priorities
1. **Z-Plane Filter System:** Core differentiating technology
2. **Sample Playback Engine:** Foundation for authentic sound
3. **Modulation Matrix:** Enables paint-gesture integration
4. **Arpeggiator Engine:** Key workflow enhancement

### Paint-Gesture Integration Opportunities
- **Filter Morphing:** Paint Y-axis controls Z-plane morphing
- **Modulation Control:** Paint pressure/velocity as modulation sources
- **Pattern Generation:** Paint strokes create arpeggiator patterns
- **Real-Time Control:** Canvas becomes modulation destination

### Hybrid Processing Integration
- **Digital Core:** Authentic Audity 2000 sample + Z-plane processing
- **Analog Character:** Post-processing via EMUFilter + TubeStage chain
- **Blend Control:** EMU Tone knob controls hybrid processing blend
- **Performance:** Maintain real-time performance with dual processing

---

## TECHNICAL REFERENCES

### Primary Sources
- Sound on Sound EMU Audity 2000 Review (1998)
- EMU Audity 2000 OS 2.0 Addendum
- Vintage Synth Explorer Technical Specifications
- EMU ROM Format Documentation

### Patent References
- US Patent 5,391,882: "Method of controlling a time-varying digital filter"
- Related E-mu filter morphing and DSP patents
- Digital synthesis and sample playback patents

### Community Resources
- EMU ROM dump analysis and extraction tools
- Vintage synth preservation communities
- DSP algorithm implementations and optimizations

---

**Document Status:** Reference Implementation Guide  
**Last Updated:** August 26, 2025  
**Next Review:** Phase 1 Development Milestone  
**Distribution:** SpectralCanvas Pro Development Team
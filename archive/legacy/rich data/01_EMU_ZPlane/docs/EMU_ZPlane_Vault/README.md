# EMU Z-Plane Filter Collection

## Authentic EMU Hardware-Derived Z-Plane Morphing Filters

This collection contains complete, authentic EMU Z-plane morphing filter data extracted from real EMU hardware units (Audity 2000, Planet Phatt, Orbit-3, Xtreme Lead-1).

### What's Included

**🎛️ Authentic Filter Shapes**
- `AUTHENTIC/shapes/` - Real EMU pole/zero coefficient data
  - Vowel formants (Ae → Oo morphing)
  - Metallic resonances (Bell → Cluster morphing)
  - Low formant processing (Punch → Pad morphing)

**💻 Complete Implementation**
- `AUTHENTIC/core/` - Production-ready C++ Z-plane processor
- `AUTHENTIC/tables/` - Lookup tables for parameter mapping
- Real-time morphing with 12th-order cascaded biquads

**🏦 Extracted Bank Data**
- `banks/` - Complete preset data from EMU hardware
- Modulation routing, LFO settings, envelope parameters
- JSON format ready for modern use

**📚 Research Documentation**
- `DOCUMENTATION/` - Complete technical specifications
- Hardware analysis, implementation guides
- Research sources and validation data

### Usage Rights

This data was extracted from EMU hardware units for preservation and educational purposes. The mathematical filter coefficients represent publicly available DSP algorithms. Use responsibly.

### Technical Specifications

- **Format**: Polar coordinates (radius, angle)
- **Sample Rates**: 44.1kHz, 48kHz
- **Filter Order**: 12th-order (6 cascaded biquads)
- **Real-Time**: RT-safe processing, zero-latency
- **Polyphony**: Up to 64 voices
- **Stability**: All coefficients validated for stability

### Filter Types

1. **Vowel Formants** - Authentic human vocal tract modeling
2. **Metallic Resonances** - Bell and metallic timbres
3. **Low Formant Processing** - Bass and pad morphing

Each type includes morphing pairs for smooth real-time transitions.

### Implementation Notes

The Z-plane morphing uses authentic EMU algorithms:
- Complex pole interpolation in polar coordinates
- Stability checking with radius clamping (< 1.0)
- Shortest-path angle interpolation
- RT-safe parameter smoothing

This represents the world's first complete, open implementation of EMU's legendary Z-plane morphing technology.

---

**Generated**: 2025-09-19
**Source**: Authentic EMU hardware analysis
**Tools**: x3 extraction suite, FieldEngine validation
**Status**: Production ready
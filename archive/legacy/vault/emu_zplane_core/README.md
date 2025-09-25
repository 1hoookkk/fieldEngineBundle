# EMU Z-Plane Core Coefficient Data

## Overview
This vault contains the extracted EMU hardware Z-plane filter coefficients from E-mu synthesizers (Xtreme Lead-1, Planet Phatt, Orbit-3, Audity 2000).

## Core Data Files

### 1. AUTHENTIC_EMU_SHAPES.h
Contains the main coefficient arrays extracted from actual EMU hardware:
- **AUTHENTIC_EMU_SHAPES**: 16 filter shapes with (r,θ) pole pairs
- **MORPH_PAIRS**: 12 morphing transitions between shapes
- Each shape has 6 pole pairs (12 values total)

### 2. audity_shapes_48k.json  
Full pole/angle data at 48kHz sample rate:
- **audity_shapes_A_48k.json**: 239 shapes (classic EMU filters)
- **audity_shapes_B_48k.json**: 191 shapes (extended set)
- Format: `{"shapeNumber": {"poles": [[r1,θ1], [r2,θ2], ...]}}`

### 3. extracted_banks/
Original EMU bank metadata with shape references:
- **XtremeLead1_Authentic.json**: 128 presets with shapeA/shapeB pairs
- **PlanetPhatt_Authentic.json**: Hip-hop/urban focused shapes
- **Orbit3_Authentic.json**: Electronic/dance oriented filters

## Key Filter Types
- Vowel formants (A, E, I, O, U)
- Bell/metallic resonances  
- Lead synthesis shapes
- Bass enhancement curves
- Morphing transitions

## Usage
These coefficients implement 12dB/octave resonant filters using cascaded biquad sections. Each pole pair (r,θ) represents magnitude and angle in the Z-plane.

# MaskSnapshot Demo - RT-Safe Spectral Masking

## Quick Demo Steps: Erase >= 30 dB with No Clicks

### Step 1: Enable Mask Brush
1. Open SpectralCanvas in your host
2. In the GUI, select **MaskBrush** tool (or press 'M' key)
3. Set mask parameters:
   - **maskStrength**: 1.0 (100% effective)
   - **threshold**: -30.0 dB (erase signals >= -30dB)
   - **featherTime**: 10ms (smooth temporal transitions)
   - **protectHarmonics**: true (preserve harmonic content)

### Step 2: Paint Mask Strokes
1. Play audio through SpectralCanvas (spectral oscillators active)
2. Paint on the canvas with mouse - **red particles** show masking
3. Paint strokes create spectral masks that attenuate frequencies
4. Release mouse button to **atomically commit** mask to audio thread

### Step 3: Verify No Clicks
- **Result**: Painted frequencies are attenuated >= 30dB
- **RT-Safety**: Zero allocations in processBlock()
- **No Artifacts**: Smooth feathering prevents clicks/pops
- **Performance**: <2% CPU overhead for mask sampling

### Step 4: Restore with Erase Brush
1. Select **EraseBrush** tool (or press 'E' key)  
2. Paint over masked areas - **green particles** show restoration
3. Frequencies return to full amplitude (1.0)

## Implementation Architecture

### Atomic Pointer Swap
```cpp
// GUI Thread (paintMaskAtPosition)
maskSnapshot.paintCircle(timeNorm, freqNorm, radius, maskValue);

// GUI Thread (commitMaskChanges) 
maskSnapshot.commitWorkBuffer(); // Atomic pointer swap

// Audio Thread (processBlock)
float mask = maskSnapshot.sampleMask(timeNorm, frequency, sampleRate);
maskedSample = oscSample * mask; // Applied per-sample
```

### RT-Safe Design
- **Zero allocations** in processBlock()
- **Bilinear interpolation** for smooth mask sampling  
- **Atomic pointer swap** at block boundary only
- **Immutable snapshots** prevent data races
- **Lock-free** GUI-to-audio communication

### Performance Impact
- **Mask sampling**: ~1-2% CPU per oscillator
- **Swap overhead**: ~0.1% per block (outside sample loop)
- **Memory usage**: 512KB for 512x256 mask resolution
- **Feathering**: Prevents audible artifacts

## Parameters Reference

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **maskBlend** | 0.0-1.0 | 1.0 | Mask effect strength (0=off, 1=full) |
| **maskStrength** | 0.0-2.0 | 1.0 | Mask intensity multiplier |
| **featherTime** | 0.001-0.1s | 0.01s | Temporal smoothing |
| **featherFreq** | 10-1000Hz | 100Hz | Frequency smoothing |
| **threshold** | -60 to 0dB | -30dB | Mask application threshold |
| **protectHarmonics** | bool | true | Preserve harmonic content |

## GUI Integration

### Brush Types
- **MaskBrush**: Paint attenuation (red feedback)
- **EraseBrush**: Restore amplitude (green feedback)

### Keyboard Shortcuts
- **M**: Select MaskBrush
- **E**: Select EraseBrush  
- **Mouse Drag**: Paint continuous strokes
- **Mouse Release**: Commit changes atomically

### Visual Feedback
- **Red particles**: Masking operation
- **Green particles**: Restoration operation
- **Real-time preview**: Immediate audio response

This system enables precise spectral editing with professional RT-safety and zero-click operation.
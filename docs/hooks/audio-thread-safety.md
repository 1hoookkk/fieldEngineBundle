# Audio Thread Safety Hook

Prevents RT-unsafe code from entering audio processing paths by scanning for common real-time violations.

## What is Checked

### RT Forbidden Patterns
- **Memory allocation**: `new`, `delete`, `malloc`, `free`, `realloc`
- **Blocking operations**: `std::lock_guard`, `std::mutex`, `std::async`
- **I/O operations**: File streams, `printf`, `fopen`
- **Hot-path hazards**: `dynamic_cast`, `std::function`, `throw`

### Parameter Smoothing
- Detects raw APVTS parameter reads in `processBlock`
- Requires use of `juce::SmoothedValue` or similar smoothers
- Prevents parameter-induced clicks and instability

### Buffer Management
- Requires `// RT: presized in prepareToPlay` comments for vector resize operations
- Ensures all allocations happen outside the audio thread

### Denorm/NaN Protection
- Requires `ScopedNoDenormals` or FTZ mode setting
- Requires debug assertions for NaN/Inf guards

## Local Usage

```bash
# Check specific files
python .claude/hooks/audio-thread-safety.py src/plugins/PitchEnginePro/PluginProcessor.cpp

# Check all matching files
python .claude/hooks/audio-thread-safety.py --check-all

# Reduce strictness for experimental code
python .claude/hooks/audio-thread-safety.py --lenient src/experimental/test.cpp

# Use custom config
python .claude/hooks/audio-thread-safety.py --config custom_config.yml src/file.cpp
```

## Configuration

Edit `.claude/hooks/config/audio_safety.yml` to customize:
- **triggers**: File patterns to scan
- **rt_forbidden_tokens**: Forbidden patterns and regex
- **parameter_smoothing**: Required smoothers and patterns
- **reporting**: Violation limits and display options

## Integration

The hook automatically runs when editing files matching:
- `src/**/*.cpp`
- `src/**/*.h`
- `Source/**/*`

## Example Violations

```cpp
// ❌ BAD: Memory allocation in processBlock
void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) {
    float* temp = new float[buffer.getNumSamples()]; // VIOLATION
    delete[] temp; // VIOLATION
}

// ✓ GOOD: Pre-allocated in prepareToPlay
class SafeProcessor {
    std::vector<float> tempBuffer;

    void prepareToPlay(double, int samplesPerBlock) {
        tempBuffer.resize(samplesPerBlock); // RT: presized in prepareToPlay
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) {
        // Use pre-allocated buffer safely
        jassert(tempBuffer.size() >= buffer.getNumSamples());
    }
};
```

## Fix Hints

Each violation includes specific guidance:
- **Memory allocation**: "Pre-allocate in prepareToPlay() or use HeapBlock/AudioBlock"
- **Locks**: "Use lock-free programming or defer to message thread"
- **Parameter reads**: "Parameter reads in processBlock must use a smoother"
- **Dynamic cast**: "Use static_cast or virtual functions"

## Override Options

To temporarily bypass checks:
- Use `--lenient` flag for exploratory branches
- Set `strict: false` in config file
- Add patterns to `allowlist` section

The hook prioritizes catching real RT-safety bugs while minimizing false positives.
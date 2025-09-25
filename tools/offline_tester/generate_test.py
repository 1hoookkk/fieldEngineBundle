#!/usr/bin/env python3
"""Generate a simple test WAV file for testing the autotune offline processor"""

import numpy as np
import wave
import struct

def generate_test_wav(filename, duration=3, sample_rate=44100):
    """Generate a sine wave that drifts slightly flat over time"""

    num_samples = duration * sample_rate
    t = np.linspace(0, duration, num_samples)

    # Start at A4 (440Hz) and drift down to 415Hz (slightly flat)
    freq = 440 - (25 * t / duration)

    # Generate sine wave with varying frequency
    phase = np.cumsum(2 * np.pi * freq / sample_rate)
    audio = 0.5 * np.sin(phase)

    # Convert to 16-bit PCM
    audio_int16 = np.int16(audio * 32767)

    # Write WAV file
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16-bit
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(audio_int16.tobytes())

    print(f"Generated {filename}: {duration}s @ {sample_rate}Hz, drifting from 440Hz to 415Hz")

if __name__ == "__main__":
    generate_test_wav(r"C:\fieldEngineBundle\build\tester\test_input.wav")
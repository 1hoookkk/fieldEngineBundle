#!/usr/bin/env python3
"""
Golden Master Fixture Generation System

Generates reference audio files and analysis data for regression testing.
Renders impulse, sweep, and multitone tests at multiple sample rates.

Usage:
  python generate_fixtures.py --plugin <plugin_path> --output-dir fixtures/
  python generate_fixtures.py --regenerate-all
"""

import os
import sys
import json
import subprocess
import argparse
import hashlib
from pathlib import Path
from typing import Dict, List, Tuple
import numpy as np
import scipy.io.wavfile as wavfile
from datetime import datetime

class FixtureGenerator:
    def __init__(self, plugin_path: str, fixtures_dir: str = "fixtures"):
        self.plugin_path = Path(plugin_path)
        self.fixtures_dir = Path(fixtures_dir)
        self.fixtures_dir.mkdir(exist_ok=True)
        
        # Test configurations
        self.sample_rates = [44100, 48000, 96000]
        self.buffer_sizes = [64, 512, 1024]
        self.test_types = ["impulse", "sweep", "multitone"]
        
        # Create subdirectories
        for sr in self.sample_rates:
            for test_type in self.test_types:
                (self.fixtures_dir / str(sr) / test_type).mkdir(parents=True, exist_ok=True)

    def generate_all_fixtures(self) -> bool:
        """Generate all fixture combinations."""
        success = True
        total_tests = len(self.sample_rates) * len(self.test_types) * len(self.buffer_sizes)
        current_test = 0
        
        for sr in self.sample_rates:
            for test_type in self.test_types:
                for buffer_size in self.buffer_sizes:
                    current_test += 1
                    print(f"\n[{current_test}/{total_tests}] Generating {test_type} at {sr}Hz, buffer={buffer_size}")
                    
                    test_success = self._generate_single_fixture(sr, test_type, buffer_size)
                    if not test_success:
                        print(f"❌ Failed: {test_type} at {sr}Hz")
                        success = False
                    else:
                        print(f"✅ Generated: {test_type} at {sr}Hz")
        
        if success:
            self._generate_metadata()
            print(f"\n🎯 All fixtures generated successfully in {self.fixtures_dir}")
        else:
            print(f"\n❌ Some fixtures failed to generate")
        
        return success

    def _generate_single_fixture(self, sample_rate: int, test_type: str, buffer_size: int) -> bool:
        """Generate a single fixture test."""
        try:
            # Generate input test signal
            input_file = self._create_test_signal(sample_rate, test_type)
            if not input_file:
                return False
            
            # Process through plugin
            output_file = self._process_with_plugin(input_file, sample_rate, test_type, buffer_size)
            if not output_file:
                return False
            
            # Analyze output and save data
            analysis_data = self._analyze_output(output_file, sample_rate, test_type)
            if not analysis_data:
                return False
            
            # Save analysis results
            self._save_analysis_data(analysis_data, sample_rate, test_type, buffer_size)
            
            # Clean up temporary input file
            if input_file.exists():
                input_file.unlink()
            
            return True
            
        except Exception as e:
            print(f"Error generating fixture: {e}")
            return False

    def _create_test_signal(self, sample_rate: int, test_type: str) -> Path:
        """Create test signal WAV file."""
        duration = 2.0  # seconds
        samples = int(duration * sample_rate)
        t = np.linspace(0, duration, samples, endpoint=False)
        
        if test_type == "impulse":
            # Unit impulse at beginning
            signal = np.zeros(samples)
            signal[0] = 1.0
            
        elif test_type == "sweep":
            # Logarithmic frequency sweep 20Hz to Nyquist/2
            f_start = 20.0
            f_end = sample_rate / 4.0  # Conservative Nyquist limit
            signal = self._generate_log_sweep(t, f_start, f_end)
            
        elif test_type == "multitone":
            # Multiple pure tones for IMD/THD analysis
            freqs = [220, 440, 880, 1760]  # Musical intervals
            signal = np.zeros(samples)
            for freq in freqs:
                if freq < sample_rate / 2.5:  # Anti-aliasing headroom
                    signal += 0.25 * np.sin(2 * np.pi * freq * t)
        
        else:
            raise ValueError(f"Unknown test type: {test_type}")
        
        # Normalize and apply fade-in/out
        signal = self._apply_fade(signal, sample_rate)
        signal = signal * 0.5  # -6dB headroom
        
        # Save as WAV
        temp_file = Path(f"temp_input_{test_type}_{sample_rate}.wav")
        wavfile.write(str(temp_file), sample_rate, signal.astype(np.float32))
        
        return temp_file

    def _generate_log_sweep(self, t: np.ndarray, f_start: float, f_end: float) -> np.ndarray:
        """Generate logarithmic frequency sweep."""
        duration = t[-1]
        k = (f_end / f_start) ** (1 / duration)
        phi = 2 * np.pi * f_start * duration / np.log(k) * (k**t - 1)
        return np.sin(phi)

    def _apply_fade(self, signal: np.ndarray, sample_rate: int) -> np.ndarray:
        """Apply fade-in/out to avoid clicks."""
        fade_samples = int(0.01 * sample_rate)  # 10ms fade
        
        # Fade in
        signal[:fade_samples] *= np.linspace(0, 1, fade_samples)
        
        # Fade out
        signal[-fade_samples:] *= np.linspace(1, 0, fade_samples)
        
        return signal

    def _process_with_plugin(self, input_file: Path, sample_rate: int, test_type: str, buffer_size: int) -> Path:
        """Process input through the plugin."""
        output_dir = self.fixtures_dir / str(sample_rate) / test_type
        output_file = output_dir / f"{test_type}_buf{buffer_size}.wav"
        
        # Build command for offline processor
        cmd = [
            str(self.plugin_path),
            "--in", str(input_file),
            "--out", str(output_file),
            "--key", "0",  # C major
            "--scale", "Major",
            "--retune", "0.7",
            "--mode", "Track",
            "--style", "Focus",
            "--block", str(buffer_size)
        ]
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode != 0:
                print(f"Plugin execution failed: {result.stderr}")
                return None
            
            if not output_file.exists():
                print(f"Output file not created: {output_file}")
                return None
            
            return output_file
            
        except subprocess.TimeoutExpired:
            print(f"Plugin execution timed out")
            return None
        except Exception as e:
            print(f"Plugin execution error: {e}")
            return None

    def _analyze_output(self, wav_file: Path, sample_rate: int, test_type: str) -> Dict:
        """Analyze processed audio and extract metrics."""
        try:
            # Load audio file
            sr, audio_data = wavfile.read(str(wav_file))
            if sr != sample_rate:
                print(f"Sample rate mismatch: expected {sample_rate}, got {sr}")
                return None
            
            # Convert to mono if stereo
            if len(audio_data.shape) > 1:
                audio_data = np.mean(audio_data, axis=1)
            
            # Ensure float format
            if audio_data.dtype == np.int16:
                audio_data = audio_data.astype(np.float32) / 32768.0
            elif audio_data.dtype == np.int32:
                audio_data = audio_data.astype(np.float32) / 2147483648.0
            
            # Frequency domain analysis
            fft_data = np.fft.rfft(audio_data)
            freqs = np.fft.rfftfreq(len(audio_data), 1/sample_rate)
            
            # Magnitude and phase
            magnitude_db = 20 * np.log10(np.abs(fft_data) + 1e-12)
            phase_unwrapped = np.unwrap(np.angle(fft_data))
            phase_deg = np.degrees(phase_unwrapped)
            
            # Group delay (derivative of phase)
            group_delay_ms = np.zeros_like(phase_unwrapped)
            if len(phase_unwrapped) > 1:
                dphi_df = np.gradient(phase_unwrapped, freqs)
                group_delay_ms = -dphi_df / (2 * np.pi) * 1000  # Convert to ms
            
            # Basic metrics
            rms_level = np.sqrt(np.mean(audio_data ** 2))
            peak_level = np.max(np.abs(audio_data))
            dc_component = np.mean(audio_data)
            
            # Check for NaN/Inf
            has_nan = np.any(np.isnan(audio_data))
            has_inf = np.any(np.isinf(audio_data))
            
            analysis_data = {
                "test_type": test_type,
                "sample_rate": sample_rate,
                "length_samples": len(audio_data),
                "duration_sec": len(audio_data) / sample_rate,
                "rms_level_dbfs": 20 * np.log10(rms_level + 1e-12),
                "peak_level_dbfs": 20 * np.log10(peak_level + 1e-12),
                "dc_component_dbfs": 20 * np.log10(abs(dc_component) + 1e-12),
                "has_nan": has_nan,
                "has_inf": has_inf,
                "frequency_hz": freqs.tolist(),
                "magnitude_db": magnitude_db.tolist(),
                "phase_deg": phase_deg.tolist(),
                "group_delay_ms": group_delay_ms.tolist()
            }
            
            return analysis_data
            
        except Exception as e:
            print(f"Analysis error: {e}")
            return None

    def _save_analysis_data(self, analysis_data: Dict, sample_rate: int, test_type: str, buffer_size: int) -> None:
        """Save analysis data as JSON and CSV."""
        output_dir = self.fixtures_dir / str(sample_rate) / test_type
        
        # Save JSON (complete data)
        json_file = output_dir / f"{test_type}_buf{buffer_size}_analysis.json"
        with open(json_file, 'w') as f:
            json.dump(analysis_data, f, indent=2)
        
        # Save CSV (frequency domain data for easy plotting)
        csv_file = output_dir / f"{test_type}_buf{buffer_size}_analysis.csv"
        with open(csv_file, 'w') as f:
            f.write("frequency_hz,magnitude_db,phase_deg,group_delay_ms\n")
            for i in range(len(analysis_data["frequency_hz"])):
                f.write(f"{analysis_data['frequency_hz'][i]:.2f},")
                f.write(f"{analysis_data['magnitude_db'][i]:.6f},")
                f.write(f"{analysis_data['phase_deg'][i]:.6f},")
                f.write(f"{analysis_data['group_delay_ms'][i]:.6f}\n")

    def _generate_metadata(self) -> None:
        """Generate metadata file with git SHA, timestamps, etc."""
        try:
            # Get git SHA
            git_sha = subprocess.check_output(
                ["git", "rev-parse", "HEAD"], 
                text=True, 
                cwd=Path.cwd()
            ).strip()
        except:
            git_sha = "unknown"
        
        try:
            # Get bank hash if exists
            bank_files = list(Path("assets/zplane/banks").glob("*.json"))
            bank_hash = "none"
            if bank_files:
                content = "".join([f.read_text() for f in bank_files])
                bank_hash = hashlib.sha256(content.encode()).hexdigest()[:8]
        except:
            bank_hash = "unknown"
        
        metadata = {
            "generated_at": datetime.now().isoformat(),
            "git_sha": git_sha,
            "bank_hash": bank_hash,
            "plugin_version": "1.0.0",  # TODO: Extract from plugin
            "platform": sys.platform,
            "sample_rates": self.sample_rates,
            "test_types": self.test_types,
            "buffer_sizes": self.buffer_sizes
        }
        
        metadata_file = self.fixtures_dir / "metadata.json"
        with open(metadata_file, 'w') as f:
            json.dump(metadata, f, indent=2)
        
        print(f"📋 Metadata saved: git={git_sha[:8]}, bank={bank_hash}")


def main():
    parser = argparse.ArgumentParser(description="Generate golden master fixtures")
    parser.add_argument("--plugin", required=True, help="Path to offline plugin processor")
    parser.add_argument("--output-dir", default="fixtures", help="Output directory for fixtures")
    parser.add_argument("--regenerate-all", action="store_true", help="Regenerate all fixtures")
    
    args = parser.parse_args()
    
    if not Path(args.plugin).exists():
        print(f"❌ Plugin not found: {args.plugin}")
        sys.exit(1)
    
    generator = FixtureGenerator(args.plugin, args.output_dir)
    
    print(f"🎯 Generating fixtures using: {args.plugin}")
    print(f"📁 Output directory: {args.output_dir}")
    
    success = generator.generate_all_fixtures()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
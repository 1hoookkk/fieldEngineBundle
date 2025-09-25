#!/usr/bin/env python3
"""
Golden Master Fixture Validation System

Compares current plugin output against stored golden masters.
Detects regressions in magnitude, phase, THD, and basic audio metrics.

Usage:
  python validate_fixtures.py --plugin <plugin_path> --fixtures-dir fixtures/
  python validate_fixtures.py --tolerance-config tolerances.yml
"""

import os
import sys
import json
import subprocess
import argparse
import yaml
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import numpy as np
import scipy.io.wavfile as wavfile
from dataclasses import dataclass

@dataclass
class ValidationResult:
    test_name: str
    passed: bool
    metrics: Dict[str, float]
    violations: List[str]
    summary: str

@dataclass
class Tolerances:
    magnitude_avg_db: float = 0.3
    magnitude_max_db: float = 0.8
    phase_avg_deg: float = 8.0
    phase_max_deg: float = 25.0
    thd_delta_db: float = 0.2
    dc_floor_dbfs: float = -80.0
    zipper_peak_db: float = 0.3
    allow_nan: bool = False
    allow_inf: bool = False

class FixtureValidator:
    def __init__(self, plugin_path: str, fixtures_dir: str = "fixtures", config_path: Optional[str] = None):
        self.plugin_path = Path(plugin_path)
        self.fixtures_dir = Path(fixtures_dir)
        self.tolerances = self._load_tolerances(config_path)
        self.results: List[ValidationResult] = []
        
        # Ensure plugin exists
        if not self.plugin_path.exists():
            raise FileNotFoundError(f"Plugin not found: {self.plugin_path}")
        
        # Ensure fixtures directory exists
        if not self.fixtures_dir.exists():
            raise FileNotFoundError(f"Fixtures directory not found: {self.fixtures_dir}")

    def _load_tolerances(self, config_path: Optional[str]) -> Tolerances:
        """Load tolerance configuration from YAML file."""
        if config_path and Path(config_path).exists():
            try:
                with open(config_path, 'r') as f:
                    config = yaml.safe_load(f)
                    return Tolerances(**config.get('tolerances', {}))
            except Exception as e:
                print(f"Warning: Could not load tolerance config: {e}")
        
        return Tolerances()  # Use defaults

    def validate_all_fixtures(self) -> bool:
        """Validate all fixtures and return True if all pass."""
        self.results = []
        
        # Find all fixture combinations
        fixture_tests = self._discover_fixtures()
        
        if not fixture_tests:
            print("❌ No fixtures found to validate")
            return False
        
        print(f"🔍 Validating {len(fixture_tests)} fixture tests...")
        
        success_count = 0
        for i, (sample_rate, test_type, buffer_size) in enumerate(fixture_tests):
            print(f"\n[{i+1}/{len(fixture_tests)}] Validating {test_type} at {sample_rate}Hz, buffer={buffer_size}")
            
            result = self._validate_single_fixture(sample_rate, test_type, buffer_size)
            self.results.append(result)
            
            if result.passed:
                success_count += 1
                print(f"✅ PASS: {result.summary}")
            else:
                print(f"❌ FAIL: {result.summary}")
                for violation in result.violations:
                    print(f"    → {violation}")
        
        # Generate summary report
        self._generate_report()
        
        all_passed = success_count == len(fixture_tests)
        print(f"\n📊 Results: {success_count}/{len(fixture_tests)} tests passed")
        
        return all_passed

    def _discover_fixtures(self) -> List[Tuple[int, str, int]]:
        """Find all available fixture test combinations."""
        fixtures = []
        
        for sr_dir in self.fixtures_dir.glob("*"):
            if not sr_dir.is_dir() or not sr_dir.name.isdigit():
                continue
            
            sample_rate = int(sr_dir.name)
            
            for test_dir in sr_dir.glob("*"):
                if not test_dir.is_dir():
                    continue
                
                test_type = test_dir.name
                
                # Find all buffer sizes for this test
                for json_file in test_dir.glob("*_analysis.json"):
                    # Extract buffer size from filename like "impulse_buf512_analysis.json"
                    filename = json_file.stem
                    if "_buf" in filename:
                        buffer_part = filename.split("_buf")[1].split("_")[0]
                        try:
                            buffer_size = int(buffer_part)
                            fixtures.append((sample_rate, test_type, buffer_size))
                        except ValueError:
                            continue
        
        return sorted(fixtures)

    def _validate_single_fixture(self, sample_rate: int, test_type: str, buffer_size: int) -> ValidationResult:
        """Validate a single fixture against its golden master."""
        test_name = f"{test_type}_{sample_rate}Hz_buf{buffer_size}"
        
        try:
            # Load golden master data
            golden_data = self._load_golden_master(sample_rate, test_type, buffer_size)
            if not golden_data:
                return ValidationResult(
                    test_name=test_name,
                    passed=False,
                    metrics={},
                    violations=["Could not load golden master data"],
                    summary="Golden master missing"
                )
            
            # Generate current output
            current_data = self._generate_current_output(sample_rate, test_type, buffer_size)
            if not current_data:
                return ValidationResult(
                    test_name=test_name,
                    passed=False,
                    metrics={},
                    violations=["Could not generate current output"],
                    summary="Current output failed"
                )
            
            # Compare outputs
            violations = []
            metrics = {}
            
            # Basic audio checks
            violations.extend(self._check_basic_audio(current_data, metrics))
            
            # Magnitude comparison
            violations.extend(self._compare_magnitude(golden_data, current_data, metrics))
            
            # Phase comparison
            violations.extend(self._compare_phase(golden_data, current_data, metrics))
            
            # THD/distortion comparison (for multitone tests)
            if test_type == "multitone":
                violations.extend(self._compare_thd(golden_data, current_data, metrics))
            
            passed = len(violations) == 0
            summary = f"Δmag_avg={metrics.get('mag_delta_avg', 0):.3f}dB, Δphase_avg={metrics.get('phase_delta_avg', 0):.1f}°"
            
            return ValidationResult(
                test_name=test_name,
                passed=passed,
                metrics=metrics,
                violations=violations,
                summary=summary
            )
            
        except Exception as e:
            return ValidationResult(
                test_name=test_name,
                passed=False,
                metrics={},
                violations=[f"Validation error: {str(e)}"],
                summary="Internal error"
            )

    def _load_golden_master(self, sample_rate: int, test_type: str, buffer_size: int) -> Optional[Dict]:
        """Load golden master analysis data."""
        json_file = self.fixtures_dir / str(sample_rate) / test_type / f"{test_type}_buf{buffer_size}_analysis.json"
        
        if not json_file.exists():
            return None
        
        try:
            with open(json_file, 'r') as f:
                return json.load(f)
        except Exception as e:
            print(f"Error loading golden master {json_file}: {e}")
            return None

    def _generate_current_output(self, sample_rate: int, test_type: str, buffer_size: int) -> Optional[Dict]:
        """Generate current plugin output and analyze it."""
        try:
            # Create temporary test signal (same as golden master generation)
            input_file = self._create_test_signal(sample_rate, test_type)
            if not input_file:
                return None
            
            # Process through plugin
            output_file = Path(f"temp_current_{test_type}_{sample_rate}_{buffer_size}.wav")
            
            cmd = [
                str(self.plugin_path),
                "--in", str(input_file),
                "--out", str(output_file),
                "--key", "0",
                "--scale", "Major",
                "--retune", "0.7",
                "--mode", "Track",
                "--style", "Focus",
                "--block", str(buffer_size)
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode != 0 or not output_file.exists():
                return None
            
            # Analyze output (reuse analysis logic from generate_fixtures.py)
            analysis_data = self._analyze_audio_file(output_file, sample_rate, test_type)
            
            # Cleanup
            if input_file.exists():
                input_file.unlink()
            if output_file.exists():
                output_file.unlink()
            
            return analysis_data
            
        except Exception as e:
            print(f"Error generating current output: {e}")
            return None

    def _create_test_signal(self, sample_rate: int, test_type: str) -> Path:
        """Create test signal (same logic as generate_fixtures.py)."""
        duration = 2.0
        samples = int(duration * sample_rate)
        t = np.linspace(0, duration, samples, endpoint=False)
        
        if test_type == "impulse":
            signal = np.zeros(samples)
            signal[0] = 1.0
        elif test_type == "sweep":
            f_start, f_end = 20.0, sample_rate / 4.0
            k = (f_end / f_start) ** (1 / duration)
            phi = 2 * np.pi * f_start * duration / np.log(k) * (k**t - 1)
            signal = np.sin(phi)
        elif test_type == "multitone":
            freqs = [220, 440, 880, 1760]
            signal = np.zeros(samples)
            for freq in freqs:
                if freq < sample_rate / 2.5:
                    signal += 0.25 * np.sin(2 * np.pi * freq * t)
        else:
            raise ValueError(f"Unknown test type: {test_type}")
        
        # Apply fade and normalize
        fade_samples = int(0.01 * sample_rate)
        signal[:fade_samples] *= np.linspace(0, 1, fade_samples)
        signal[-fade_samples:] *= np.linspace(1, 0, fade_samples)
        signal = signal * 0.5
        
        temp_file = Path(f"temp_input_{test_type}_{sample_rate}.wav")
        wavfile.write(str(temp_file), sample_rate, signal.astype(np.float32))
        
        return temp_file

    def _analyze_audio_file(self, wav_file: Path, sample_rate: int, test_type: str) -> Dict:
        """Analyze audio file (same logic as generate_fixtures.py)."""
        sr, audio_data = wavfile.read(str(wav_file))
        
        if len(audio_data.shape) > 1:
            audio_data = np.mean(audio_data, axis=1)
        
        if audio_data.dtype == np.int16:
            audio_data = audio_data.astype(np.float32) / 32768.0
        elif audio_data.dtype == np.int32:
            audio_data = audio_data.astype(np.float32) / 2147483648.0
        
        fft_data = np.fft.rfft(audio_data)
        freqs = np.fft.rfftfreq(len(audio_data), 1/sample_rate)
        
        magnitude_db = 20 * np.log10(np.abs(fft_data) + 1e-12)
        phase_unwrapped = np.unwrap(np.angle(fft_data))
        phase_deg = np.degrees(phase_unwrapped)
        
        group_delay_ms = np.zeros_like(phase_unwrapped)
        if len(phase_unwrapped) > 1:
            dphi_df = np.gradient(phase_unwrapped, freqs)
            group_delay_ms = -dphi_df / (2 * np.pi) * 1000
        
        rms_level = np.sqrt(np.mean(audio_data ** 2))
        peak_level = np.max(np.abs(audio_data))
        dc_component = np.mean(audio_data)
        
        return {
            "test_type": test_type,
            "sample_rate": sample_rate,
            "length_samples": len(audio_data),
            "rms_level_dbfs": 20 * np.log10(rms_level + 1e-12),
            "peak_level_dbfs": 20 * np.log10(peak_level + 1e-12),
            "dc_component_dbfs": 20 * np.log10(abs(dc_component) + 1e-12),
            "has_nan": bool(np.any(np.isnan(audio_data))),
            "has_inf": bool(np.any(np.isinf(audio_data))),
            "frequency_hz": freqs.tolist(),
            "magnitude_db": magnitude_db.tolist(),
            "phase_deg": phase_deg.tolist(),
            "group_delay_ms": group_delay_ms.tolist()
        }

    def _check_basic_audio(self, data: Dict, metrics: Dict[str, float]) -> List[str]:
        """Check basic audio properties."""
        violations = []
        
        # NaN/Inf checks
        if data.get("has_nan") and not self.tolerances.allow_nan:
            violations.append("Audio contains NaN values")
        
        if data.get("has_inf") and not self.tolerances.allow_inf:
            violations.append("Audio contains Inf values")
        
        # DC component check
        dc_dbfs = data.get("dc_component_dbfs", 0)
        metrics["dc_component_dbfs"] = dc_dbfs
        if dc_dbfs > self.tolerances.dc_floor_dbfs:
            violations.append(f"DC component too high: {dc_dbfs:.1f} dBFS (limit: {self.tolerances.dc_floor_dbfs:.1f} dBFS)")
        
        return violations

    def _compare_magnitude(self, golden: Dict, current: Dict, metrics: Dict[str, float]) -> List[str]:
        """Compare magnitude response."""
        violations = []
        
        golden_mag = np.array(golden["magnitude_db"])
        current_mag = np.array(current["magnitude_db"])
        
        # Ensure same length (interpolate if needed)
        if len(golden_mag) != len(current_mag):
            # Simple approach: truncate to shorter length
            min_len = min(len(golden_mag), len(current_mag))
            golden_mag = golden_mag[:min_len]
            current_mag = current_mag[:min_len]
        
        delta_mag = np.abs(current_mag - golden_mag)
        
        avg_delta = np.mean(delta_mag)
        max_delta = np.max(delta_mag)
        
        metrics["mag_delta_avg"] = avg_delta
        metrics["mag_delta_max"] = max_delta
        
        if avg_delta > self.tolerances.magnitude_avg_db:
            violations.append(f"Magnitude average delta {avg_delta:.3f} dB > {self.tolerances.magnitude_avg_db:.3f} dB")
        
        if max_delta > self.tolerances.magnitude_max_db:
            violations.append(f"Magnitude max delta {max_delta:.3f} dB > {self.tolerances.magnitude_max_db:.3f} dB")
        
        return violations

    def _compare_phase(self, golden: Dict, current: Dict, metrics: Dict[str, float]) -> List[str]:
        """Compare phase response."""
        violations = []
        
        golden_phase = np.array(golden["phase_deg"])
        current_phase = np.array(current["phase_deg"])
        
        if len(golden_phase) != len(current_phase):
            min_len = min(len(golden_phase), len(current_phase))
            golden_phase = golden_phase[:min_len]
            current_phase = current_phase[:min_len]
        
        # Handle phase wrapping
        delta_phase = np.abs(current_phase - golden_phase)
        delta_phase = np.minimum(delta_phase, 360 - delta_phase)  # Handle wrap-around
        
        avg_delta = np.mean(delta_phase)
        max_delta = np.max(delta_phase)
        
        metrics["phase_delta_avg"] = avg_delta
        metrics["phase_delta_max"] = max_delta
        
        if avg_delta > self.tolerances.phase_avg_deg:
            violations.append(f"Phase average delta {avg_delta:.1f}° > {self.tolerances.phase_avg_deg:.1f}°")
        
        if max_delta > self.tolerances.phase_max_deg:
            violations.append(f"Phase max delta {max_delta:.1f}° > {self.tolerances.phase_max_deg:.1f}°")
        
        return violations

    def _compare_thd(self, golden: Dict, current: Dict, metrics: Dict[str, float]) -> List[str]:
        """Compare THD for multitone tests."""
        violations = []
        
        # Simple THD estimation - compare energy at fundamental vs harmonic frequencies
        # This is a basic implementation; could be enhanced with more sophisticated analysis
        
        golden_mag = np.array(golden["magnitude_db"])
        current_mag = np.array(current["magnitude_db"])
        
        # Calculate approximate THD as ratio of peak energy to average energy
        golden_thd = np.max(golden_mag) - np.mean(golden_mag)
        current_thd = np.max(current_mag) - np.mean(current_mag)
        
        thd_delta = abs(current_thd - golden_thd)
        metrics["thd_delta_db"] = thd_delta
        
        if thd_delta > self.tolerances.thd_delta_db:
            violations.append(f"THD delta {thd_delta:.3f} dB > {self.tolerances.thd_delta_db:.3f} dB")
        
        return violations

    def _generate_report(self) -> None:
        """Generate validation report files."""
        reports_dir = Path("reports/bench")
        reports_dir.mkdir(parents=True, exist_ok=True)
        
        # Summary text report
        summary_file = reports_dir / "summary.txt"
        with open(summary_file, 'w') as f:
            f.write("Golden Master Fixture Validation Report\n")
            f.write("=" * 45 + "\n\n")
            
            passed_count = sum(1 for r in self.results if r.passed)
            f.write(f"Results: {passed_count}/{len(self.results)} tests passed\n\n")
            
            for result in self.results:
                status = "PASS" if result.passed else "FAIL"
                f.write(f"[{status}] {result.test_name}: {result.summary}\n")
                
                if not result.passed:
                    for violation in result.violations:
                        f.write(f"  → {violation}\n")
            
            f.write(f"\nOverall: {'PASS' if passed_count == len(self.results) else 'FAIL'}\n")
        
        # Detailed CSV report
        csv_file = reports_dir / "validation_details.csv"
        with open(csv_file, 'w') as f:
            f.write("test_name,passed,mag_delta_avg,mag_delta_max,phase_delta_avg,phase_delta_max,dc_component_dbfs\n")
            
            for result in self.results:
                f.write(f"{result.test_name},{result.passed},")
                f.write(f"{result.metrics.get('mag_delta_avg', 0):.6f},")
                f.write(f"{result.metrics.get('mag_delta_max', 0):.6f},")
                f.write(f"{result.metrics.get('phase_delta_avg', 0):.6f},")
                f.write(f"{result.metrics.get('phase_delta_max', 0):.6f},")
                f.write(f"{result.metrics.get('dc_component_dbfs', 0):.6f}\n")
        
        print(f"📊 Reports saved to {reports_dir}/")


def main():
    parser = argparse.ArgumentParser(description="Validate fixtures against golden masters")
    parser.add_argument("--plugin", required=True, help="Path to offline plugin processor")
    parser.add_argument("--fixtures-dir", default="fixtures", help="Directory containing fixtures")
    parser.add_argument("--tolerance-config", help="YAML file with tolerance settings")
    
    args = parser.parse_args()
    
    try:
        validator = FixtureValidator(args.plugin, args.fixtures_dir, args.tolerance_config)
        success = validator.validate_all_fixtures()
        sys.exit(0 if success else 1)
        
    except Exception as e:
        print(f"❌ Validation failed: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
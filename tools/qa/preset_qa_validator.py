#!/usr/bin/env python3
"""
Z-plane Preset QA Validator
Validates JSON schemas, creates fingerprints, and tests round-trip compatibility
"""

import json
import hashlib
import os
import sys
from pathlib import Path
from typing import Dict, List, Any, Tuple
from datetime import datetime
import argparse

class PresetQAValidator:
    def __init__(self):
        self.base_dir = Path(__file__).parent.parent.parent
        self.errors = []
        self.warnings = []
        self.stats = {
            'files_processed': 0,
            'presets_validated': 0,
            'schema_errors': 0,
            'fingerprints_generated': 0,
            'round_trip_tests': 0,
            'round_trip_failures': 0
        }

    def log_error(self, message: str):
        self.errors.append(message)
        print(f"ERROR: {message}")

    def log_warning(self, message: str):
        self.warnings.append(message)
        print(f"WARNING: {message}")

    def log_info(self, message: str):
        print(f"INFO: {message}")

    def find_zplane_files(self) -> List[Path]:
        """Find all Z-plane related JSON files"""
        zplane_files = []

        # Search patterns for Z-plane files
        search_paths = [
            self.base_dir / "archive" / "legacy" / "vault" / "emu_zplane" / "banks",
            self.base_dir / "archive" / "legacy" / "rich data" / "01_EMU_ZPlane" / "src" / "emu_extracted" / "banks",
            self.base_dir / "plugins" / "_shared" / "zplane",
            self.base_dir / "plugins" / "morphengine" / "assets" / "zplane",
            self.base_dir / "tools" / "params",
        ]

        for path in search_paths:
            if path.exists():
                for file in path.glob("*.json"):
                    zplane_files.append(file)

        return zplane_files

    def validate_json_schema(self, file_path: Path) -> Tuple[bool, Dict[str, Any]]:
        """Validate JSON file structure and return parsed data"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            self.stats['files_processed'] += 1
            return True, data

        except json.JSONDecodeError as e:
            self.log_error(f"Invalid JSON in {file_path}: {e}")
            self.stats['schema_errors'] += 1
            return False, {}
        except Exception as e:
            self.log_error(f"Failed to read {file_path}: {e}")
            self.stats['schema_errors'] += 1
            return False, {}

    def validate_morphing_model_schema(self, data: Dict[str, Any], file_path: Path) -> bool:
        """Validate morphing model JSON schema"""
        required_fields = {
            'version': str,
            'sampleRateRef': (int, float),
            'description': str,
            'models': dict
        }

        valid = True

        # Check top-level structure
        for field, expected_type in required_fields.items():
            if field not in data:
                self.log_error(f"{file_path}: Missing required field '{field}'")
                valid = False
            elif not isinstance(data[field], expected_type):
                self.log_error(f"{file_path}: Field '{field}' should be {expected_type}, got {type(data[field])}")
                valid = False

        # Validate models structure
        if 'models' in data:
            for model_id, model_data in data['models'].items():
                if not self.validate_model_structure(model_data, model_id, file_path):
                    valid = False

        return valid

    def validate_model_structure(self, model: Dict[str, Any], model_id: str, file_path: Path) -> bool:
        """Validate individual model structure"""
        required_fields = ['id', 'name', 'description', 'frameA', 'frameB']
        valid = True

        for field in required_fields:
            if field not in model:
                self.log_error(f"{file_path}: Model '{model_id}' missing field '{field}'")
                valid = False

        # Validate frame structures
        for frame_name in ['frameA', 'frameB']:
            if frame_name in model:
                frame = model[frame_name]
                if 'name' not in frame:
                    self.log_error(f"{file_path}: {frame_name} in model '{model_id}' missing 'name'")
                    valid = False
                if 'poles' not in frame:
                    self.log_error(f"{file_path}: {frame_name} in model '{model_id}' missing 'poles'")
                    valid = False
                elif not isinstance(frame['poles'], list):
                    self.log_error(f"{file_path}: {frame_name} 'poles' should be list")
                    valid = False
                else:
                    # Validate pole structure
                    for i, pole in enumerate(frame['poles']):
                        if not isinstance(pole, dict) or 'r' not in pole or 'theta' not in pole:
                            self.log_error(f"{file_path}: Invalid pole structure at index {i} in {frame_name}")
                            valid = False

        return valid

    def validate_bank_schema(self, data: Dict[str, Any], file_path: Path) -> bool:
        """Validate bank JSON schema"""
        valid = True

        # Check for meta section
        if 'meta' not in data:
            self.log_warning(f"{file_path}: Missing 'meta' section")
        else:
            meta = data['meta']
            if 'bank' not in meta:
                self.log_warning(f"{file_path}: Meta section missing 'bank' field")

        # Check for presets
        if 'presets' not in data:
            self.log_error(f"{file_path}: Missing 'presets' array")
            valid = False
        elif not isinstance(data['presets'], list):
            self.log_error(f"{file_path}: 'presets' should be array")
            valid = False
        else:
            self.stats['presets_validated'] += len(data['presets'])
            for i, preset in enumerate(data['presets']):
                if not self.validate_preset_structure(preset, i, file_path):
                    valid = False

        return valid

    def validate_preset_structure(self, preset: Dict[str, Any], index: int, file_path: Path) -> bool:
        """Validate individual preset structure"""
        valid = True

        if 'name' not in preset:
            self.log_error(f"{file_path}: Preset {index} missing 'name'")
            valid = False

        # Validate modulation arrays
        if 'mods' in preset:
            if not isinstance(preset['mods'], list):
                self.log_error(f"{file_path}: Preset {index} 'mods' should be array")
                valid = False
            else:
                for j, mod in enumerate(preset['mods']):
                    if not isinstance(mod, dict):
                        self.log_error(f"{file_path}: Invalid mod structure at preset {index}, mod {j}")
                        valid = False
                    elif not all(key in mod for key in ['src', 'dst', 'depth']):
                        self.log_error(f"{file_path}: Mod {j} in preset {index} missing required fields")
                        valid = False

        return valid

    def create_fingerprint(self, data: Dict[str, Any], file_path: Path) -> str:
        """Create a fingerprint for preset identification"""
        try:
            # Create a normalized representation for hashing
            if 'models' in data:
                # For model files, hash the model structure
                fingerprint_data = {
                    'version': data.get('version'),
                    'sampleRateRef': data.get('sampleRateRef'),
                    'models': {}
                }

                for model_id, model in data['models'].items():
                    fingerprint_data['models'][model_id] = {
                        'frameA_poles': len(model.get('frameA', {}).get('poles', [])),
                        'frameB_poles': len(model.get('frameB', {}).get('poles', [])),
                        'name': model.get('name', '')
                    }
            else:
                # For bank files, hash meta + preset count + structure
                fingerprint_data = {
                    'bank': data.get('meta', {}).get('bank', 'unknown'),
                    'preset_count': len(data.get('presets', [])),
                    'has_mods': any('mods' in p for p in data.get('presets', [])),
                    'mod_sources': set()
                }

                # Collect unique modulation sources
                for preset in data.get('presets', []):
                    for mod in preset.get('mods', []):
                        fingerprint_data['mod_sources'].add(mod.get('src', ''))

                fingerprint_data['mod_sources'] = sorted(list(fingerprint_data['mod_sources']))

            # Create hash
            fingerprint_json = json.dumps(fingerprint_data, sort_keys=True)
            fingerprint = hashlib.sha256(fingerprint_json.encode()).hexdigest()[:16]

            self.stats['fingerprints_generated'] += 1
            return fingerprint

        except Exception as e:
            self.log_error(f"Failed to create fingerprint for {file_path}: {e}")
            return "invalid"

    def test_round_trip(self, data: Dict[str, Any], file_path: Path) -> bool:
        """Test JSON round-trip compatibility"""
        try:
            self.stats['round_trip_tests'] += 1

            # Serialize and parse back
            json_str = json.dumps(data, indent=2, sort_keys=True)
            parsed_back = json.loads(json_str)

            # Compare structures
            if self.deep_compare(data, parsed_back):
                return True
            else:
                self.log_error(f"{file_path}: Round-trip test failed - data mismatch")
                self.stats['round_trip_failures'] += 1
                return False

        except Exception as e:
            self.log_error(f"{file_path}: Round-trip test failed with exception: {e}")
            self.stats['round_trip_failures'] += 1
            return False

    def deep_compare(self, obj1, obj2) -> bool:
        """Deep compare two objects for equality"""
        if type(obj1) != type(obj2):
            return False

        if isinstance(obj1, dict):
            if set(obj1.keys()) != set(obj2.keys()):
                return False
            return all(self.deep_compare(obj1[k], obj2[k]) for k in obj1.keys())
        elif isinstance(obj1, list):
            if len(obj1) != len(obj2):
                return False
            return all(self.deep_compare(obj1[i], obj2[i]) for i in range(len(obj1)))
        else:
            return obj1 == obj2

    def generate_report(self) -> Dict[str, Any]:
        """Generate comprehensive QA report"""

        self.log_info("Scanning for Z-plane JSON files...")
        zplane_files = self.find_zplane_files()

        if not zplane_files:
            self.log_error("No Z-plane JSON files found")
            return {}

        self.log_info(f"Found {len(zplane_files)} Z-plane JSON files")

        file_results = []
        fingerprints = {}

        for file_path in zplane_files:
            self.log_info(f"Processing {file_path.name}...")

            # Validate JSON schema
            is_valid, data = self.validate_json_schema(file_path)
            if not is_valid:
                continue

            # Determine file type and validate accordingly
            is_model_file = 'models' in data
            is_bank_file = 'presets' in data

            schema_valid = True
            if is_model_file:
                schema_valid = self.validate_morphing_model_schema(data, file_path)
            elif is_bank_file:
                schema_valid = self.validate_bank_schema(data, file_path)
            else:
                self.log_warning(f"{file_path}: Unknown file type - neither model nor bank")

            # Create fingerprint
            fingerprint = self.create_fingerprint(data, file_path)
            if fingerprint in fingerprints:
                self.log_warning(f"Duplicate fingerprint detected: {file_path.name} matches {fingerprints[fingerprint]}")
            else:
                fingerprints[fingerprint] = file_path.name

            # Test round-trip compatibility
            round_trip_ok = self.test_round_trip(data, file_path)

            file_results.append({
                'file': str(file_path.relative_to(self.base_dir)),
                'file_size': file_path.stat().st_size,
                'type': 'model' if is_model_file else 'bank' if is_bank_file else 'unknown',
                'valid_json': is_valid,
                'valid_schema': schema_valid,
                'fingerprint': fingerprint,
                'round_trip_ok': round_trip_ok,
                'preset_count': len(data.get('presets', [])) if is_bank_file else len(data.get('models', {})) if is_model_file else 0
            })

        # Generate final report
        report = {
            'timestamp': datetime.now().isoformat(),
            'summary': {
                'total_files': len(zplane_files),
                'valid_files': len([r for r in file_results if r['valid_json'] and r['valid_schema']]),
                'schema_errors': len([r for r in file_results if not r['valid_schema']]),
                'round_trip_failures': len([r for r in file_results if not r['round_trip_ok']]),
                'unique_fingerprints': len(fingerprints),
                'duplicate_fingerprints': len(file_results) - len(fingerprints)
            },
            'statistics': self.stats,
            'files': file_results,
            'fingerprints': fingerprints,
            'errors': self.errors,
            'warnings': self.warnings
        }

        return report

    def save_report(self, report: Dict[str, Any], output_file: Path):
        """Save the QA report to file"""
        try:
            with open(output_file, 'w', encoding='utf-8') as f:
                json.dump(report, f, indent=2, sort_keys=True)
            self.log_info(f"QA report saved to {output_file}")
        except Exception as e:
            self.log_error(f"Failed to save report: {e}")

def main():
    parser = argparse.ArgumentParser(description='Z-plane Preset QA Validator')
    parser.add_argument('--output', '-o', type=str, default='preset_qa_report.json',
                      help='Output file for QA report')
    parser.add_argument('--verbose', '-v', action='store_true',
                      help='Verbose output')

    args = parser.parse_args()

    validator = PresetQAValidator()

    print("=" * 60)
    print("Z-plane Preset QA Validator")
    print("=" * 60)

    report = validator.generate_report()

    if report:
        # Save report
        output_path = Path(args.output)
        validator.save_report(report, output_path)

        # Print summary
        print("\n" + "=" * 60)
        print("QA SUMMARY")
        print("=" * 60)

        summary = report['summary']
        print(f"Total Files Processed: {summary['total_files']}")
        print(f"Valid Files: {summary['valid_files']}")
        print(f"Schema Errors: {summary['schema_errors']}")
        print(f"Round-trip Failures: {summary['round_trip_failures']}")
        print(f"Unique Fingerprints: {summary['unique_fingerprints']}")
        print(f"Duplicate Fingerprints: {summary['duplicate_fingerprints']}")

        stats = report['statistics']
        print(f"\nPresets Validated: {stats['presets_validated']}")
        print(f"Fingerprints Generated: {stats['fingerprints_generated']}")
        print(f"Round-trip Tests: {stats['round_trip_tests']}")

        if validator.errors:
            print(f"\nErrors: {len(validator.errors)}")
            if args.verbose:
                for error in validator.errors:
                    print(f"  - {error}")

        if validator.warnings:
            print(f"Warnings: {len(validator.warnings)}")
            if args.verbose:
                for warning in validator.warnings:
                    print(f"  - {warning}")

        # Determine overall result
        if summary['schema_errors'] == 0 and summary['round_trip_failures'] == 0:
            print("\n✅ ALL TESTS PASSED")
            return 0
        else:
            print("\n❌ SOME TESTS FAILED")
            return 1
    else:
        print("❌ No valid data found")
        return 1

if __name__ == "__main__":
    sys.exit(main())
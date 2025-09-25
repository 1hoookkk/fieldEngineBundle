#!/usr/bin/env python3
"""
EXB Bank Extraction Tool
Extracts samples and preset data from EXB files for Z-plane morphing system
"""

import os
import sys
import struct
import json
import re
from pathlib import Path
from typing import Dict, List, Tuple, Optional

class EBLExtractor:
    """Extract and convert .ebl files to WAV format"""

    def __init__(self):
        self.ebl_header_size = 24  # Standard EBL header size

    def extract_ebl_to_wav(self, ebl_path: str, output_dir: str) -> Optional[str]:
        """Convert EBL file to WAV format"""
        try:
            with open(ebl_path, 'rb') as f:
                data = f.read()

            # EBL format detection - look for audio data signature
            if len(data) < self.ebl_header_size:
                return None

            # Skip EBL header and extract raw audio data
            audio_data = data[self.ebl_header_size:]

            if len(audio_data) < 100:  # Skip very small files
                return None

            # Create WAV header for 16-bit mono 44.1kHz
            sample_rate = 44100
            bits_per_sample = 16
            num_channels = 1
            byte_rate = sample_rate * num_channels * bits_per_sample // 8
            block_align = num_channels * bits_per_sample // 8
            data_size = len(audio_data)

            wav_header = struct.pack('<4sI4s4sIHHIIHH4sI',
                b'RIFF',
                36 + data_size,
                b'WAVE',
                b'fmt ',
                16,  # PCM format chunk size
                1,   # PCM format
                num_channels,
                sample_rate,
                byte_rate,
                block_align,
                bits_per_sample,
                b'data',
                data_size
            )

            # Generate clean filename
            filename = Path(ebl_path).stem.replace('Xtreme Lead-1SL', 'sample_')
            wav_path = os.path.join(output_dir, f"{filename}.wav")

            with open(wav_path, 'wb') as f:
                f.write(wav_header)
                f.write(audio_data)

            return wav_path

        except Exception as e:
            print(f"Warning: Failed to convert {ebl_path}: {e}")
            return None

class EXBPresetExtractor:
    """Extract preset/instrument data from main EXB file"""

    def __init__(self):
        self.preset_patterns = [
            rb'[A-Za-z0-9 ]{8,32}',  # Preset names
            rb'Filter',               # Filter sections
            rb'LFO',                  # LFO sections
            rb'ENV',                  # Envelope sections
        ]

    def find_text_regions(self, data: bytes, min_length: int = 6) -> List[Tuple[int, str]]:
        """Find printable text regions in binary data"""
        text_regions = []
        current_text = ""
        start_pos = 0

        for i, byte in enumerate(data):
            if 32 <= byte <= 126:  # Printable ASCII
                if not current_text:
                    start_pos = i
                current_text += chr(byte)
            else:
                if len(current_text) >= min_length:
                    text_regions.append((start_pos, current_text))
                current_text = ""

        return text_regions

    def extract_preset_data(self, exb_path: str) -> Dict:
        """Extract preset definitions from EXB file"""
        try:
            with open(exb_path, 'rb') as f:
                data = f.read()

            # Find text regions that might contain preset names
            text_regions = self.find_text_regions(data)

            presets = []
            for offset, text in text_regions:
                # Look for preset-like names
                if any(keyword in text.lower() for keyword in ['lead', 'bass', 'pad', 'arp', 'seq']):
                    # Extract surrounding binary data for parameter analysis
                    context_start = max(0, offset - 512)
                    context_end = min(len(data), offset + len(text) + 512)
                    context_data = data[context_start:context_end]

                    preset = {
                        'name': text.strip(),
                        'offset': offset,
                        'context_hex': context_data.hex(),
                        'parameters': self.extract_parameters_from_context(context_data)
                    }
                    presets.append(preset)

            return {
                'bank_name': 'ExtractedBank',
                'format': 'Z-plane',
                'presets': presets[:64]  # Limit to reasonable number
            }

        except Exception as e:
            print(f"Error extracting preset data: {e}")
            return {'bank_name': 'ExtractedBank', 'format': 'Z-plane', 'presets': []}

    def extract_parameters_from_context(self, context_data: bytes) -> Dict:
        """Extract filter parameters from binary context"""
        params = {
            'morph': 0.5,
            'intensity': 0.6,
            'drive': 0.3,
            'autoMakeup': True,
            'shape': 'Vowel_Ae'
        }

        # Look for parameter-like byte patterns
        # This is heuristic - would need reverse engineering for exact mapping
        for i in range(0, len(context_data) - 4, 4):
            value = struct.unpack('<f', context_data[i:i+4])[0] if len(context_data[i:i+4]) == 4 else 0

            # Map to reasonable parameter ranges
            if 0.0 <= value <= 1.0:
                if i % 16 == 0:
                    params['morph'] = value
                elif i % 16 == 4:
                    params['intensity'] = value
                elif i % 16 == 8:
                    params['drive'] = value

        return params

class ZPlanePresetGenerator:
    """Generate Z-plane compatible presets from extracted data"""

    def __init__(self):
        self.shape_templates = [
            'Vowel_Ae', 'Vowel_Eh', 'Vowel_Ih', 'Vowel_Oh', 'Vowel_Oo',
            'Bell_Metallic', 'Bell_Glass', 'Bell_Warm',
            'Formant_Male', 'Formant_Female',
            'Lead_Bright', 'Lead_Warm', 'Lead_Aggressive'
        ]

    def generate_preset(self, preset_data: Dict, sample_files: List[str]) -> Dict:
        """Generate Z-plane preset from extracted data"""
        name = preset_data.get('name', 'Untitled').replace(' ', '_')
        params = preset_data.get('parameters', {})

        # Select shape based on name heuristics
        shape = 'Vowel_Ae'  # Default
        name_lower = name.lower()
        if 'lead' in name_lower:
            shape = 'Lead_Bright'
        elif 'bass' in name_lower:
            shape = 'Vowel_Oh'
        elif 'bell' in name_lower or 'pluck' in name_lower:
            shape = 'Bell_Metallic'
        elif 'pad' in name_lower:
            shape = 'Vowel_Oo'

        preset = {
            'name': name,
            'type': 'Z-plane',
            'parameters': {
                'morph': params.get('morph', 0.5),
                'intensity': params.get('intensity', 0.6),
                'drive': params.get('drive', 0.3),
                'autoMakeup': params.get('autoMakeup', True),
                'shape': shape,
                'sectionSaturation': [0.2, 0.2, 0.2, 0.2, 0.2, 0.2]
            },
            'samples': sample_files[:4] if sample_files else [],  # Limit to 4 samples
            'metadata': {
                'source': 'EXB',
                'extracted': True,
                'original_name': preset_data.get('name', ''),
                'offset': preset_data.get('offset', 0)
            }
        }

        return preset

def main():
    if len(sys.argv) < 2:
        print("Usage: python extract_exb.py <path_to_exb_folder> [output_dir]")
        print("Example: python extract_exb.py 'C:\\path\\to\\Xtreme Lead-1.exb' ./extracted")
        sys.exit(1)

    exb_folder = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else './extracted_bank'

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    samples_dir = os.path.join(output_dir, 'samples')
    os.makedirs(samples_dir, exist_ok=True)

    print(f"Extracting EXB data from: {exb_folder}")
    print(f"Output directory: {output_dir}")

    # Find main EXB file and sample pool
    exb_file = None
    sample_pool_dir = None

    for item in os.listdir(exb_folder):
        item_path = os.path.join(exb_folder, item)
        if item.endswith('.exb') and os.path.isfile(item_path):
            exb_file = item_path
        elif item == 'SamplePool' and os.path.isdir(item_path):
            sample_pool_dir = item_path

    if not exb_file:
        print("Error: No .exb file found in directory")
        sys.exit(1)

    if not sample_pool_dir:
        print("Error: No SamplePool directory found")
        sys.exit(1)

    # Extract samples from EBL files
    print("Extracting samples...")
    ebl_extractor = EBLExtractor()
    sample_files = []

    ebl_files = [f for f in os.listdir(sample_pool_dir) if f.endswith('.ebl')][:32]  # Limit samples
    for i, ebl_file in enumerate(ebl_files):
        ebl_path = os.path.join(sample_pool_dir, ebl_file)
        wav_path = ebl_extractor.extract_ebl_to_wav(ebl_path, samples_dir)
        if wav_path:
            sample_files.append(os.path.basename(wav_path))
            if i % 10 == 0:
                print(f"  Processed {i}/{len(ebl_files)} samples...")

    print(f"Extracted {len(sample_files)} samples")

    # Extract presets from main EXB file
    print("Extracting presets...")
    preset_extractor = EXBPresetExtractor()
    preset_data = preset_extractor.extract_preset_data(exb_file)

    # Generate Z-plane compatible presets
    print("Generating Z-plane presets...")
    generator = ZPlanePresetGenerator()

    final_presets = []
    for preset in preset_data['presets']:
        zplane_preset = generator.generate_preset(preset, sample_files)
        final_presets.append(zplane_preset)

    # Create final bank file
    bank = {
        'name': 'ExtractedBank',
        'format': 'Z-plane',
        'version': '1.0',
        'presets': final_presets,
        'samples': sample_files,
        'metadata': {
            'source': 'EXB',
            'extracted_at': os.path.basename(exb_folder),
            'total_samples': len(sample_files),
            'total_presets': len(final_presets)
        }
    }

    # Save bank file
    bank_file = os.path.join(output_dir, 'extracted_bank.json')
    with open(bank_file, 'w') as f:
        json.dump(bank, f, indent=2)

    print(f"\nExtraction complete!")
    print(f"Bank file: {bank_file}")
    print(f"Samples: {samples_dir}")
    print(f"Total presets: {len(final_presets)}")
    print(f"Total samples: {len(sample_files)}")

if __name__ == '__main__':
    main()
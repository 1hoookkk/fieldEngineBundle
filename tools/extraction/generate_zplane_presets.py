#!/usr/bin/env python3
"""
Generate morphing filter presets from extracted samples
Creates musically useful presets for the morphing filter system
"""

import json
import os
import sys
from pathlib import Path
from typing import List, Dict

class MorphingPresetGenerator:
    """Generate authentic morphing filter presets"""

    def __init__(self):
        self.shape_library = {
            # Vowel shapes for vocal-style morphing
            'Vowel_Ae': {'type': 'vowel', 'character': 'bright', 'q_scale': 3.0},
            'Vowel_Eh': {'type': 'vowel', 'character': 'mid', 'q_scale': 2.5},
            'Vowel_Ih': {'type': 'vowel', 'character': 'nasal', 'q_scale': 4.0},
            'Vowel_Oh': {'type': 'vowel', 'character': 'warm', 'q_scale': 2.0},
            'Vowel_Oo': {'type': 'vowel', 'character': 'dark', 'q_scale': 1.5},

            # Bell shapes for percussive/metallic sounds
            'Bell_Metallic': {'type': 'bell', 'character': 'bright', 'q_scale': 6.0},
            'Bell_Glass': {'type': 'bell', 'character': 'crystalline', 'q_scale': 8.0},
            'Bell_Warm': {'type': 'bell', 'character': 'warm', 'q_scale': 4.0},

            # Lead shapes for aggressive sounds
            'Lead_Bright': {'type': 'lead', 'character': 'cutting', 'q_scale': 5.0},
            'Lead_Warm': {'type': 'lead', 'character': 'smooth', 'q_scale': 3.0},
            'Lead_Aggressive': {'type': 'lead', 'character': 'harsh', 'q_scale': 7.0},

            # Formant shapes for speech-like qualities
            'Formant_Male': {'type': 'formant', 'character': 'deep', 'q_scale': 2.8},
            'Formant_Female': {'type': 'formant', 'character': 'high', 'q_scale': 3.5},
        }

        self.preset_templates = [
            # Mysterious pole-based presets
            {
                'name': 'Pole Drift',
                'description': 'Drifting pole configurations',
                'shape': 'Lead_Bright',
                'morph': 0.3,
                'intensity': 0.7,
                'drive': 0.4,
                'category': 'Lead'
            },
            {
                'name': 'Pole Scatter',
                'description': 'Scattered pole arrangements',
                'shape': 'Vowel_Ae',
                'morph': 0.6,
                'intensity': 0.8,
                'drive': 0.3,
                'category': 'Lead'
            },
            {
                'name': 'Pole Sweep',
                'description': 'Sweeping pole trajectories',
                'shape': 'Bell_Metallic',
                'morph': 0.2,
                'intensity': 0.9,
                'drive': 0.5,
                'category': 'Lead'
            },
            {
                'name': 'Pole Bend',
                'description': 'Bent pole positioning',
                'shape': 'Lead_Aggressive',
                'morph': 0.1,
                'intensity': 1.0,
                'drive': 0.7,
                'category': 'Lead'
            },
            {
                'name': 'Pole Warp',
                'description': 'Warped pole geometry',
                'shape': 'Lead_Warm',
                'morph': 0.4,
                'intensity': 0.8,
                'drive': 0.5,
                'category': 'Lead'
            },

            # Bass presets
            {
                'name': 'Pole Twist',
                'description': 'Twisted pole structures',
                'shape': 'Formant_Male',
                'morph': 0.4,
                'intensity': 0.6,
                'drive': 0.6,
                'category': 'Bass'
            },
            {
                'name': 'Pole Crawl',
                'description': 'Crawling pole movement',
                'shape': 'Vowel_Oh',
                'morph': 0.7,
                'intensity': 0.5,
                'drive': 0.4,
                'category': 'Bass'
            },

            # Pad presets
            {
                'name': 'Pole Blur',
                'description': 'Blurred pole boundaries',
                'shape': 'Bell_Glass',
                'morph': 0.5,
                'intensity': 0.4,
                'drive': 0.2,
                'category': 'Pad'
            },
            {
                'name': 'Pole Walker',
                'description': 'Walking pole configurations',
                'shape': 'Vowel_Oo',
                'morph': 0.8,
                'intensity': 0.3,
                'drive': 0.25,
                'category': 'Pad'
            },

            # Pluck presets
            {
                'name': 'Pole Flow',
                'description': 'Flowing pole dynamics',
                'shape': 'Bell_Warm',
                'morph': 0.3,
                'intensity': 0.8,
                'drive': 0.3,
                'category': 'Pluck'
            },
            {
                'name': 'Pole Prowl',
                'description': 'Prowling pole behavior',
                'shape': 'Formant_Female',
                'morph': 0.4,
                'intensity': 0.9,
                'drive': 0.4,
                'category': 'Pluck'
            },

            # Arp presets
            {
                'name': 'Pole Climb',
                'description': 'Climbing pole arrangements',
                'shape': 'Bell_Metallic',
                'morph': 0.35,
                'intensity': 0.75,
                'drive': 0.35,
                'category': 'Arp'
            },
        ]

    def assign_samples_to_preset(self, samples: List[str], preset_template: Dict) -> List[str]:
        """Intelligently assign samples to preset based on category"""
        if not samples:
            return []

        category = preset_template.get('category', 'Lead')
        sample_count = min(4, len(samples))  # Limit to 4 samples per preset

        # Sort samples by ID to get consistent assignment
        sorted_samples = sorted(samples)

        if category == 'Bass':
            # Use lower-numbered samples for bass (typically lower pitch)
            return sorted_samples[:sample_count]
        elif category == 'Lead':
            # Use mid-range samples for leads
            start_idx = len(sorted_samples) // 4
            return sorted_samples[start_idx:start_idx + sample_count]
        elif category == 'Pad':
            # Use higher-numbered samples for pads (typically more complex)
            start_idx = len(sorted_samples) // 2
            return sorted_samples[start_idx:start_idx + sample_count]
        elif category == 'Pluck':
            # Use bright samples for plucks
            start_idx = len(sorted_samples) * 3 // 4
            return sorted_samples[start_idx:start_idx + sample_count]
        else:  # FX
            # Use varied samples for FX
            step = max(1, len(sorted_samples) // sample_count)
            return [sorted_samples[i * step] for i in range(sample_count)]

    def generate_section_saturation(self, shape_info: Dict, intensity: float, drive: float) -> List[float]:
        """Generate per-section saturation values based on shape and parameters"""
        base_saturation = drive * 0.5  # Scale drive to saturation range
        q_scale = shape_info.get('q_scale', 3.0)
        character = shape_info.get('character', 'mid')

        # Adjust saturation based on character
        if character in ['bright', 'cutting', 'harsh']:
            # More saturation in higher sections
            saturations = [
                base_saturation * 0.8,
                base_saturation * 1.0,
                base_saturation * 1.2,
                base_saturation * 1.4,
                base_saturation * 1.3,
                base_saturation * 1.1
            ]
        elif character in ['warm', 'deep']:
            # More saturation in lower sections
            saturations = [
                base_saturation * 1.4,
                base_saturation * 1.2,
                base_saturation * 1.0,
                base_saturation * 0.8,
                base_saturation * 0.6,
                base_saturation * 0.7
            ]
        else:  # Mid, balanced
            # Even saturation distribution
            saturations = [base_saturation * 1.0] * 6

        # Apply intensity scaling
        intensity_scale = 0.5 + (intensity * 0.5)  # 0.5 to 1.0 range
        saturations = [s * intensity_scale for s in saturations]

        # Clamp to valid range
        return [max(0.0, min(1.0, s)) for s in saturations]

    def create_preset(self, template: Dict, samples: List[str]) -> Dict:
        """Create a complete Z-plane preset from template"""
        shape = template['shape']
        shape_info = self.shape_library.get(shape, self.shape_library['Vowel_Ae'])

        # Assign samples intelligently
        preset_samples = self.assign_samples_to_preset(samples, template)

        # Generate section saturation
        section_saturation = self.generate_section_saturation(
            shape_info,
            template['intensity'],
            template['drive']
        )

        preset = {
            'name': template['name'],
            'description': template['description'],
            'type': 'Z-plane',
            'category': template.get('category', 'Lead'),
            'parameters': {
                'morph': template['morph'],
                'intensity': template['intensity'],
                'drive': template['drive'],
                'autoMakeup': True,
                'shape': shape,
                'sectionSaturation': section_saturation
            },
            'samples': preset_samples,
            'metadata': {
                'source': 'Generated',
                'shape_character': shape_info['character'],
                'q_scale': shape_info['q_scale'],
                'generator_version': '1.0'
            }
        }

        return preset

    def generate_bank(self, samples: List[str], bank_name: str = 'MorphingBank') -> Dict:
        """Generate morphing filter bank structure with samples and shape library"""

        bank = {
            'name': bank_name,
            'format': 'morphing-filter',
            'version': '1.0',
            'description': 'Extracted samples ready for morphing filter experimentation',
            'presets': [],  # Empty - encourage manual tweaking
            'samples': samples,
            'shapes': self.shape_library,
            'suggested_names': [template['name'] for template in self.preset_templates],
            'metadata': {
                'generator': 'MorphingPresetGenerator',
                'total_samples': len(samples),
                'shape_count': len(self.shape_library),
                'generated_from': 'EXB extraction',
                'notes': 'No presets included - manual parameter exploration encouraged'
            }
        }

        return bank

def main():
    if len(sys.argv) < 2:
        print("Usage: python generate_zplane_presets.py <extracted_bank.json> [output_file]")
        print("Example: python generate_zplane_presets.py extracted_xtreme/extracted_bank.json xtreme_zplane_bank.json")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'zplane_bank.json'

    # Load extracted bank
    try:
        with open(input_file, 'r') as f:
            extracted_data = json.load(f)
    except Exception as e:
        print(f"Error loading input file: {e}")
        sys.exit(1)

    samples = extracted_data.get('samples', [])
    if not samples:
        print("Error: No samples found in input file")
        sys.exit(1)

    print(f"Generating morphing filter presets from {len(samples)} samples...")

    # Generate presets
    generator = MorphingPresetGenerator()
    bank_name = f"Morphing_{len(samples)}s"
    bank = generator.generate_bank(samples, bank_name)

    # Save bank
    try:
        with open(output_file, 'w') as f:
            json.dump(bank, f, indent=2)
        print(f"Generated Z-plane bank: {output_file}")
        print(f"Total presets: {len(bank['presets'])}")
        print(f"Sample assignments: {len(samples)} samples across {len(bank['presets'])} presets")

        # Print preset summary
        categories = {}
        for preset in bank['presets']:
            cat = preset.get('category', 'Unknown')
            categories[cat] = categories.get(cat, 0) + 1

        print("\nPreset categories:")
        for cat, count in categories.items():
            print(f"  {cat}: {count} presets")

    except Exception as e:
        print(f"Error saving output file: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
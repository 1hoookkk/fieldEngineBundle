#!/usr/bin/env python3
"""
Generate 0x22 filter parameter requests for discovered EMU ROM IDs.
Creates SysEx messages to request filter parameters from EMU devices.
"""

import sys
import argparse
from pathlib import Path

# Add project root to path
ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

# Constants from the E-MU Proteus Family SysEx Specification
MANUFACTURER_ID = 0x18  # E-MU Systems
PRODUCT_ID = 0x0F       # Proteus Family
COMMAND_PRESET_DUMP_REQUEST = 0x10
SUBCOMMAND_LAYER_FILTER_PARAMS_DUMP_REQUEST = 0x22

def u16_to_bytes(value: int) -> tuple[int, int]:
    """Converts a 14-bit unsigned integer to two 7-bit bytes (LSB first)."""
    lsb = value & 0x7F
    msb = (value >> 7) & 0x7F
    return lsb, msb

def generate_filter_param_request(
    device_id: int,
    preset_number: int,
    layer_number: int,
    rom_id: int
) -> bytes:
    """
    Generates a SysEx message to request Preset Layer Filter Parameters.
    Format: F0 18 0F dd 55 10 22 xx xx yy yy zz zz F7
    Where:
        dd       = Device ID
        xx xx    = Preset Number (LSB first)
        yy yy    = Layer Number (LSB first)
        zz zz    = Preset ROM ID (LSB first)
    """
    preset_lsb, preset_msb = u16_to_bytes(preset_number)
    layer_lsb, layer_msb = u16_to_bytes(layer_number)
    rom_lsb, rom_msb = u16_to_bytes(rom_id)

    message = bytes([
        0xF0,
        MANUFACTURER_ID,
        PRODUCT_ID,
        device_id,
        0x55, # Special Editor designator byte (fixed value)
        COMMAND_PRESET_DUMP_REQUEST,
        SUBCOMMAND_LAYER_FILTER_PARAMS_DUMP_REQUEST,
        preset_lsb, preset_msb,
        layer_lsb, layer_msb,
        rom_lsb, rom_msb,
        0xF7
    ])
    return message

def main():
    parser = argparse.ArgumentParser(
        description="Generate SysEx messages to request E-MU Preset Layer Filter Parameters.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate requests for ROM 0x0FC0 (Audity/XTREM) with 4 layers per preset
  python generate_filter_requests.py 0x0FC0 0 --max-presets 10 --max-layers 4
  
  # Generate requests for all discovered ROM IDs
  python generate_filter_requests.py --all-roms --device-id 0
        """
    )
    
    parser.add_argument("rom_id", nargs='?', type=lambda x: int(x, 0), 
                       help="The ROM ID to request (e.g., 0x0FC0). Use --all-roms to generate for all discovered ROMs.")
    parser.add_argument("device_id", nargs='?', type=lambda x: int(x, 0), default=0,
                       help="The Device ID to target (default: 0).")
    parser.add_argument("--max-presets", type=int, default=10, 
                       help="Maximum number of presets to request (default: 10).")
    parser.add_argument("--max-layers", type=int, default=4, 
                       help="Maximum number of layers per preset to request (default: 4).")
    parser.add_argument("--all-roms", action="store_true",
                       help="Generate requests for all discovered ROM IDs.")
    parser.add_argument("--output-dir", type=Path, default=Path("filter_requests"),
                       help="Directory to save request files (default: filter_requests/).")
    
    args = parser.parse_args()
    
    # Discovered ROM IDs from our parser
    discovered_roms = {
        0x0A14: "XROM_Kit_Presets",
        0x0B40: "Unknown_ROM_1", 
        0x0F80: "Unknown_ROM_2",
        0x0FC0: "Audity_XTREM_Presets"
    }
    
    if args.all_roms:
        rom_ids = list(discovered_roms.keys())
        print(f"Generating requests for all discovered ROM IDs: {[f'0x{rid:04X}' for rid in rom_ids]}")
    else:
        if args.rom_id is None:
            parser.error("Either specify a ROM ID or use --all-roms")
        rom_ids = [args.rom_id]
    
    device_id = args.device_id
    max_presets = args.max_presets
    max_layers = args.max_layers
    output_dir = args.output_dir
    
    # Create output directory
    output_dir.mkdir(exist_ok=True)
    
    total_requests = 0
    
    for rom_id in rom_ids:
        rom_name = discovered_roms.get(rom_id, f"ROM_{rom_id:04X}")
        print(f"\nCreating filter parameter requests for ROM {rom_id:04X} ({rom_name})")
        
        all_requests = []
        for preset_num in range(max_presets):
            for layer_num in range(max_layers):
                request_msg = generate_filter_param_request(
                    device_id=device_id,
                    preset_number=preset_num,
                    layer_number=layer_num,
                    rom_id=rom_id
                )
                all_requests.append(request_msg)
        
        # Save to file
        output_filename = output_dir / f"filter_requests_rom_{rom_id:04X}_device_{device_id}.syx"
        with open(output_filename, "wb") as f:
            for req in all_requests:
                f.write(req)
        
        print(f"  Created {len(all_requests)} filter parameter requests")
        print(f"  Saved to: {output_filename}")
        total_requests += len(all_requests)
    
    print(f"\n[SUCCESS] Generated {total_requests} total filter parameter requests")
    print(f"Files saved in: {output_dir}")
    print("\nNext steps:")
    print("1. Send these .syx files to your EMU device")
    print("2. Capture the responses (filter parameter dumps)")
    print("3. Use the enhanced parser to extract filter_type_id values")
    print("4. Map filter_type_id to AUTHENTIC_EMU_SHAPES")

if __name__ == "__main__":
    main()

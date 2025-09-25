#!/usr/bin/env python3
"""
Script to request filter parameters from EMU devices.
This sends SysEx requests for filter parameter dumps.
"""

import sys
import time
from pathlib import Path

def create_filter_request(preset_num: int, layer_num: int, rom_id: int, device_id: int = 0) -> bytes:
    """
    Create a Preset Layer Filter Params Dump Request message.
    
    Args:
        preset_num: Preset number (0-127)
        layer_num: Layer number (0-3)
        rom_id: ROM ID (0-255)
        device_id: Device ID (0-15)
    
    Returns:
        SysEx message bytes
    """
    # F0 18 0F dd 55 10 22 xx xx yy yy zz zz F7
    msg = bytearray([
        0xF0,  # SysEx start
        0x18,  # EMU ID
        0x0F,  # Proteus ID
        device_id,  # Device ID
        0x55,  # Command byte
        0x10,  # Preset Dump family
        0x22,  # Filter Params Dump Request
        preset_num & 0x7F, (preset_num >> 7) & 0x7F,  # Preset number (LSB first)
        layer_num & 0x7F, (layer_num >> 7) & 0x7F,    # Layer number (LSB first)
        rom_id & 0x7F, (rom_id >> 7) & 0x7F,          # ROM ID (LSB first)
        0xF7   # SysEx end
    ])
    return bytes(msg)

def create_bulk_filter_requests(rom_id: int, device_id: int = 0) -> list:
    """
    Create filter parameter requests for all presets and layers in a ROM.
    
    Args:
        rom_id: ROM ID to request from
        device_id: Device ID
    
    Returns:
        List of SysEx messages
    """
    requests = []
    
    # Request from all presets (0-127) and all layers (0-3)
    for preset in range(128):
        for layer in range(4):
            msg = create_filter_request(preset, layer, rom_id, device_id)
            requests.append(msg)
    
    return requests

def save_requests_to_file(requests: list, filename: str):
    """Save SysEx requests to a file."""
    with open(filename, 'wb') as f:
        for msg in requests:
            f.write(msg)
            f.write(b'\n')  # Add newline for readability

def main():
    if len(sys.argv) < 2:
        print("Usage: request_filter_params.py <rom_id> [device_id]")
        print("Example: request_filter_params.py 0 0")
        sys.exit(1)
    
    rom_id = int(sys.argv[1])
    device_id = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    
    print(f"Creating filter parameter requests for ROM ID {rom_id}, Device ID {device_id}")
    
    # Create requests for all presets and layers
    requests = create_bulk_filter_requests(rom_id, device_id)
    
    # Save to file
    filename = f"filter_requests_rom_{rom_id}_device_{device_id}.syx"
    save_requests_to_file(requests, filename)
    
    print(f"Created {len(requests)} filter parameter requests")
    print(f"Saved to: {filename}")
    print(f"Send these messages to your EMU device to get filter parameter dumps")

if __name__ == "__main__":
    main()

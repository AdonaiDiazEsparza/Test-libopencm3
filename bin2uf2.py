#!/usr/bin/env python3

from __future__ import annotations

import argparse
import struct
from pathlib import Path

UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID = 0x00002000
UF2_PAYLOAD = 256

# Microsoft uf2 family IDs (utils/uf2families.json). First match wins.
UF2_FAMILIES = (
    ("stm32f411?e", 0x2DC309C5),  # STM32F411xE (512K)
    ("stm32f411e", 0x2DC309C5),
    ("stm32f411?c", 0x06D1097B),  # STM32F411xC (256K)
    ("stm32f411c", 0x06D1097B),
    ("stm32f4", 0x57755A57),
    ("stm32f1", 0x5EE21072),
    ("stm32f0", 0x647824B6),
    ("stm32f2", 0x5D1A0A2E),
    ("stm32f3", 0x6B846188),
    ("stm32f7", 0x53B80F00),
    ("stm32g0", 0x300F5633),
    ("stm32g4", 0x4C71240A),
    ("stm32h7", 0x6DB66082),
    ("stm32l0", 0x202E3A91),
    ("stm32l1", 0x1E1F432D),
    ("stm32l4", 0x00FF6919),
    ("stm32l5", 0x04240BDF),
    ("rp2040", 0xE48BFF56),
)


def _fnmatch_device(device: str, pattern: str) -> bool:
    device = device.lower()
    pattern = pattern.lower()
    if "?" not in pattern:
        return device.startswith(pattern)
    # One-character wildcard, then the rest of the pattern as a prefix of the tail.
    head, tail = pattern.split("?", 1)
    if not device.startswith(head):
        return False
    rest = device[len(head) :]
    return len(rest) >= 1 + len(tail) and rest[1:].startswith(tail)


def family_id_for_device(device: str) -> int:
    for pattern, family in UF2_FAMILIES:
        if _fnmatch_device(device, pattern):
            return family
    return 0


def bin_to_uf2(blob: bytes, base: int, family: int) -> bytes:
    flags = UF2_FLAG_FAMILY_ID if family else 0
    padding = b"\x00" * (512 - 32 - UF2_PAYLOAD - 4)
    nblocks = max(1, (len(blob) + UF2_PAYLOAD - 1) // UF2_PAYLOAD)
    blocks = []
    for blockno in range(nblocks):
        ptr = blockno * UF2_PAYLOAD
        chunk = blob[ptr : ptr + UF2_PAYLOAD].ljust(UF2_PAYLOAD, b"\x00")
        header = struct.pack(
            "<IIIIIIII",
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            flags,
            base + ptr,
            UF2_PAYLOAD,
            blockno,
            nblocks,
            family,
        )
        block = header + chunk + padding + struct.pack("<I", UF2_MAGIC_END)
        assert len(block) == 512
        blocks.append(block)
    return b"".join(blocks)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binfile", type=Path, help="Input .bin")
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output .uf2")
    parser.add_argument(
        "--base",
        default="0x08000000",
        help="Flash load address (default: 0x08000000)",
    )
    parser.add_argument(
        "--family",
        default=None,
        help="UF2 family id as hex (e.g. 0x2dc309c5). Overrides --device.",
    )
    parser.add_argument(
        "--device",
        default="",
        help="MCU part name used to pick a family id (e.g. stm32f411ceu6)",
    )
    args = parser.parse_args()

    if args.family is not None:
        family = int(args.family, 0)
    elif args.device:
        family = family_id_for_device(args.device)
    else:
        family = 0

    blob = args.binfile.read_bytes()
    uf2 = bin_to_uf2(blob, int(args.base, 0), family)
    args.output.write_bytes(uf2)


if __name__ == "__main__":
    main()

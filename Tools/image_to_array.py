#!/usr/bin/env python3

import argparse
import re
from pathlib import Path


def sanitize_name(name: str) -> str:
    name = re.sub(r"\W+", "_", name)
    if re.match(r"^\d", name):
        name = "_" + name
    return name


def image_to_c_array(input_path: Path, output_path: Path | None, var_name: str | None):
    data = input_path.read_bytes()

    if var_name is None:
        var_name = sanitize_name(input_path.stem + "_" + input_path.suffix[1:])

    lines = []

    lines.append(f"unsigned char {var_name}[] = {{")

    bytes_per_line = 12
    for i in range(0, len(data), bytes_per_line):
        chunk = data[i : i + bytes_per_line]
        byte_values = ", ".join(f"0x{byte:02x}" for byte in chunk)
        lines.append(f"    {byte_values},")

    lines.append("};")
    lines.append(f"unsigned int {var_name}_len = {len(data)};")
    lines.append("")

    output = "\n".join(lines)

    if output_path:
        output_path.write_text(output)
    else:
        print(output)


def main():
    parser = argparse.ArgumentParser(
        description="Convert an image or binary file into a C byte array."
    )

    parser.add_argument("input", help="Input image file")
    parser.add_argument(
        "-o",
        "--output",
        help="Output .h or .c file. If omitted, prints to stdout.",
    )
    parser.add_argument(
        "-n",
        "--name",
        help="Variable name for the byte array. Defaults to filename-based name.",
    )

    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output) if args.output else None

    image_to_c_array(input_path, output_path, args.name)


if __name__ == "__main__":
    main()
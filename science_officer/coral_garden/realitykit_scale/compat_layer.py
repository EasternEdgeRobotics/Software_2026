import argparse
import os, sys
from pathlib import Path

def parse_args():
    parser = argparse.ArgumentParser(description="Upload images from BlueStar to WebODM")

    parser.add_argument(
        "--image-dir",
        default=os.getenv("IMAGE_DIR", "/home/peyton/test-set"),
        help="Directory containing images to upload",
    )

    parser.add_argument(
        "--section",
        default=os.getenv("PHOTO_SECTION", ""),
        help="Photo section name, such as section_0",
    )

    return parser.parse_args()

def main():
    args = parse_args()
    print("RealityKit Compatability Layer")

    image_dir = Path(args.image_dir)
    session_dir = image_dir.parent
    session_name = session_dir.name
    section_name = image_dir.name

    realitykit_scale_location = f"{Path.home()}/Developer/Eastern Edge/Software_2026/science_officer/coral_garden/realitykit_scale/.build/release/realitykit_scale"
    output_path = f"{Path.home()}/Pictures/bluestar_scans/{session_name}/{section_name}/"
    print(realitykit_scale_location)

    command = [
        str(realitykit_scale_location),
        "--input",
        str(image_dir),
        "--detail",
        "full",
        "--sequential",
        "--high-sensitivity",
        "--checkpoint",
        "./checkpoints",
        "--output",
        str(output_path)
    ]

    os.execv(str(realitykit_scale_location), command)

    return 1

if __name__ == "__main__":
    sys.exit(main())
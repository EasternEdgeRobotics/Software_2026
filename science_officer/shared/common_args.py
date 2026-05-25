import argparse

def video_args(parser: argparse.ArgumentParser) -> argparse.ArgumentParser:
    parser.add_argument(
        "--source-type",
        default="video",
        help="video or usb"
    )

    parser.add_argument(
        "--source",
        default="rtsp://192.168.137.200:8554/cam",
        help="Video source (ie usb0, rtsp://192.168.137.200:8554/cam)"
    )

    parser.add_argument(
        "--resolution",
        default="1280x720",
        help="Source resolution (required for FFmpeg backend)"
    )

    parser.add_argument(
        "--capture-backend",
        default="opencv",
        choices=["opencv", "ffmpeg"],
        help="Frame capture backend. Use ffmpeg for RTSP",
    )

    parser.add_argument(
        "--ffmpeg-loglevel",
        default="error",
        help="FFmpeg loglevel: quiet, error, warning, info, debug",
    )

    return parser

def fisheye_args(parser: argparse.ArgumentParser) -> argparse.ArgumentParser:
    parser.add_argument(
        "--fisheye-correction",
        default=False,
        action="store_true",
        help="Enable Fisheye correction",
    )

    parser.add_argument(
        "--fisheye-type",
        default="pinhole",
        choices=["pinhole", "fisheye"],
        help="Select fisheye type",
    )

    return parser

def measurement_args(parser: argparse.ArgumentParser) -> argparse.ArgumentParser:
    parser.add_argument(
        "--follow-mouse",
        default=True,
        action="store_true",
        help="Draw a line from the previous point to the cursor",
    )

    return parser
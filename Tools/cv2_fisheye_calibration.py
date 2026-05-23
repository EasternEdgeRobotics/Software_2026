import cv2
from math import sqrt
import os
import numpy as np
import time
import sys
from pathlib import Path
import argparse

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = "rtsp_transport;tcp|max_delay;0"

from science_officer.shared import frame_capture
from science_officer.shared import opencv_helpers
from science_officer.shared import common_args

def parse_args():
    parser = argparse.ArgumentParser(description="Calibrate BlueStar")

    common_args.video_args(parser)

    parser.add_argument(
        "--checker-board-width",
        type=int,
        default=11,
        help="INNER Width of the checkerboard pattern used for calibration. if total = 12 then inner = 11",
    )

    parser.add_argument(
        "--checker-board-height",
        type=int,
        default=8,
        help="INNER Height of the checkerboard pattern used for calibration. if total = 9 then inner = 8",
    )

    parser.add_argument(
        "--checker-size",
        type=float,
        default=20.0,
        help="size of individual checkers? (idk what the term for a single square on a checkerboard is) used for calibration in mm",
    )

    parser.add_argument(
        "--samples",
        type=int,
        default=25,
        help="Number of accepted checkerboard images to collect before calibration.",
    )

    parser.add_argument(
        "--show-undistorted",
        action="store_true",
        help="After calibration, show an undistorted preview.",
    )

    return parser.parse_args()

def print_calibration_results(K, D, rms, image_size):
    print("\n================ CAMERA CALIBRATION RESULT ================")
    print(f"RMS reprojection error: {rms}")
    print(f"Image size: {image_size[0]} x {image_size[1]}")

    print("\nK = np.array(")
    print(np.array2string(K, separator=", ", precision=12))
    print(", dtype=np.float64)")

    print("\nD = np.array(")
    print(np.array2string(D, separator=", ", precision=12))
    print(", dtype=np.float64)")

    print("\nFlattened D, if your config wants a simple list:")
    print(np.array2string(D.ravel(), separator=", ", precision=12))

    print("\nYAML-ish format:")
    print("camera_matrix:")
    print(f"  rows: {K.shape[0]}")
    print(f"  cols: {K.shape[1]}")
    print(f"  data: {K.ravel().tolist()}")
    print("distortion_coefficients:")
    print(f"  rows: {D.shape[0]}")
    print(f"  cols: {D.shape[1] if len(D.shape) > 1 else 1}")
    print(f"  data: {D.ravel().tolist()}")
    print("===========================================================\n")


def main():
    args = parse_args()

    resize = False
    resW = None
    resH = None

    if args.resolution:
        resize = True
        resW, resH = (
            int(args.resolution.split("x")[0]),
            int(args.resolution.split("x")[1]),
        )

    frame_source = None

    if args.source_type in ["video", "usb"]:
        if args.capture_backend == "ffmpeg":
            if args.source_type == "usb":
                print("ERROR: FFmpeg backend is intended for video/RTSP sources")
                print("Use --capture-backend opencv for USB cameras.")
                sys.exit(1)

            if not args.resolution:
                print("ERROR: FFmpeg backend requires --resolution WIDTHxHEIGHT.")
                sys.exit(1)

            frame_source = frame_capture.FFmpegFrameSource(
                url=args.source,
                width=resW,
                height=resH,
                loglevel=args.ffmpeg_loglevel,
            )
            frame_source.start()

        else:
            frame_source = frame_capture.OpenCVFrameSource(
                source=args.source,
                source_type=args.source_type,
                width=resW if args.resolution else None,
                height=resH if args.resolution else None,
            )

    if frame_source is None:
        print("ERROR: No valid frame source was created.")
        sys.exit(1)

    checkerboard_size = (
        args.checker_board_width,
        args.checker_board_height,
    )

    square_size = args.checker_size

    print("\nCalibration settings:")
    print(f"  Checkerboard inner corners: {checkerboard_size}")
    print(f"  Checker size: {square_size}")
    print(f"  Required samples: {args.samples}")
    print("\nControls:")
    print("  space = accept current checkerboard frame")
    print("  c     = accept current checkerboard frame")
    print("  q/esc = quit without calibrating")
    print("\nMove the board around:")
    print("  - center, corners, edges")
    print("  - tilted left/right/up/down")
    print("  - close and far")
    print("  - fill much of the frame in some samples\n")

    # Prepare object points.
    # Example for 12x9 inner corners:
    # [(0,0,0), (1,0,0), ..., (11,8,0)] * square_size
    objp = np.zeros(
        (checkerboard_size[0] * checkerboard_size[1], 3),
        np.float32,
    )
    objp[:, :2] = np.mgrid[
        0 : checkerboard_size[0],
        0 : checkerboard_size[1],
    ].T.reshape(-1, 2)

    objp *= square_size

    object_points = []
    image_points = []

    last_gray = None
    image_size = None

    criteria = (
        cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER,
        30,
        0.001,
    )

    flags = (
        cv2.CALIB_CB_ADAPTIVE_THRESH
        + cv2.CALIB_CB_NORMALIZE_IMAGE
        + cv2.CALIB_CB_FAST_CHECK
    )

    while len(object_points) < args.samples:
        ret, frame = frame_source.read()

        if not ret or frame is None:
            print("Error: Failed to capture frame.")
            time.sleep(0.1)
            continue

        if resize:
            frame = cv2.resize(frame, (resW, resH))

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        last_gray = gray
        image_size = gray.shape[::-1]

        found, corners = cv2.findChessboardCorners(
            gray,
            checkerboard_size,
            flags,
        )

        display = frame.copy()

        if found:
            corners_subpix = cv2.cornerSubPix(
                gray,
                corners,
                (11, 11),
                (-1, -1),
                criteria,
            )

            cv2.drawChessboardCorners(
                display,
                checkerboard_size,
                corners_subpix,
                found,
            )

            status = "CHECKERBOARD FOUND - press SPACE or C to capture"
            color = (0, 255, 0)
        else:
            corners_subpix = None
            status = "checkerboard not found"
            color = (0, 0, 255)
        
        cv2.putText(
            display,
            status,
            (20, 35),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            color,
            2,
            cv2.LINE_AA,
        )

        cv2.putText(
            display,
            f"Samples: {len(object_points)}/{args.samples}",
            (20, 70),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            (255, 255, 255),
            2,
            cv2.LINE_AA,
        )

        cv2.imshow("Calibration", display)

        key = cv2.waitKey(1) & 0xFF

        if key in [ord("q"), 27]:
            print("Calibration cancelled.")
            cv2.destroyAllWindows()
            sys.exit(0)

        if key in [ord(" "), ord("c")]:
            if found and corners_subpix is not None:
                object_points.append(objp.copy())
                image_points.append(corners_subpix.copy())

                print(
                    f"Accepted sample {len(object_points)}/{args.samples}"
                )
            else:
                print("Cannot accept sample: checkerboard not found.")

    cv2.destroyWindow("Calibration")

    if len(object_points) < 3:
        print("ERROR: Not enough samples for calibration.")
        sys.exit(1)

    print("\nCalibrating camera...")

    rms, K, D, rvecs, tvecs = cv2.calibrateCamera(
        object_points,
        image_points,
        image_size,
        None,
        None,
    )

    print_calibration_results(K, D, rms, image_size)

    if args.show_undistorted:
        print("Showing undistorted preview. Press q or esc to quit.")

        while True:
            ret, frame = frame_source.read()

            if not ret or frame is None:
                print("Error: Failed to capture frame.")
                time.sleep(0.1)
                continue

            if resize:
                frame = cv2.resize(frame, (resW, resH))

            h, w = frame.shape[:2]

            new_K, roi = cv2.getOptimalNewCameraMatrix(
                K,
                D,
                (w, h),
                1,
                (w, h),
            )

            undistorted = cv2.undistort(frame, K, D, None, new_K)

            cv2.imshow("Original", frame)
            cv2.imshow("Undistorted", undistorted)

            key = cv2.waitKey(1) & 0xFF

            if key in [ord("q"), 27]:
                break

    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()

import cv2
from math import sqrt
import os
import numpy as np
import time
import sys
from pathlib import Path
import argparse
import json

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

    parser.add_argument(
        "--calibration-set-dir",
        type=str,
        default=None,
        help=(
            "Directory to save accepted calibration samples. "
            "If omitted, a timestamped directory will be created."
        ),
    )

    parser.add_argument(
        "--load-calibration-set",
        type=str,
        default=None,
        help=(
            "Load a previously saved calibration set instead of capturing "
            "new samples."
        ),
    )

    parser.add_argument(
        "--no-save-calibration-set",
        action="store_true",
        help="Disable saving accepted calibration samples.",
    )

    return parser.parse_args()

def print_pinhole_calibration_results(K, D, rms, image_size):
    D_flat = D.ravel()

    print("\n================ PINHOLE CALIBRATION RESULT ================")
    print(f"RMS reprojection error: {rms}")
    print(f"Image size: {image_size[0]} x {image_size[1]}")

    print("\n# Pinhole/OpenCV normal camera matrix")
    print("PINHOLE_K = np.array(")
    print(np.array2string(K, separator=", ", precision=12))
    print(", dtype=np.float64)")

    print("\n# Pinhole/OpenCV normal distortion model")
    print("# Layout is usually: [k1, k2, p1, p2, k3]")
    print("# Use with cv2.calibrateCamera/cv2.undistort")
    print("# Do NOT use this with cv2.fisheye functions.")
    print("PINHOLE_D = np.array(")
    print(np.array2string(D_flat, separator=", ", precision=12))
    print(", dtype=np.float64)")


def print_fisheye_calibration_results(K, D, rms, image_size):
    D_flat = D.ravel()

    print("\n================ FISHEYE CALIBRATION RESULT ================")
    print(f"RMS reprojection error: {rms}")
    print(f"Image size: {image_size[0]} x {image_size[1]}")

    print("\n# Fisheye camera matrix")
    print("FISHEYE_K = np.array(")
    print(np.array2string(K, separator=", ", precision=12))
    print(", dtype=np.float64)")

    print("\n# OpenCV fisheye distortion model")
    print("# Layout: [k1, k2, k3, k4]")
    print("# Use with cv2.fisheye functions.")
    print("# This is NOT the same as the first 4 values of pinhole D.")
    print("FISHEYE_D = np.array(")
    print(np.array2string(D_flat, separator=", ", precision=12))
    print(", dtype=np.float64)")

def calibrate_pinhole(object_points, image_points, image_size):
    rms, K, D, rvecs, tvecs = cv2.calibrateCamera(
        object_points,
        image_points,
        image_size,
        None,
        None,
    )

    return rms, K, D, rvecs, tvecs


def calibrate_fisheye(object_points, image_points, image_size):
    width, height = image_size

    object_points_fisheye = []
    image_points_fisheye = []

    for obj, img in zip(object_points, image_points):
        obj = np.asarray(obj, dtype=np.float64).reshape(-1, 1, 3)
        img = np.asarray(img, dtype=np.float64).reshape(-1, 1, 2)

        object_points_fisheye.append(obj)
        image_points_fisheye.append(img)

    # Give the fisheye solver a reasonable starting camera matrix.
    # This makes cv2.fisheye.calibrate much less fragile.
    focal_guess = max(width, height) / 2.0

    K = np.array(
        [
            [focal_guess, 0.0, width / 2.0],
            [0.0, focal_guess, height / 2.0],
            [0.0, 0.0, 1.0],
        ],
        dtype=np.float64,
    )

    D = np.zeros((4, 1), dtype=np.float64)

    flags = (
        cv2.fisheye.CALIB_USE_INTRINSIC_GUESS
        + cv2.fisheye.CALIB_RECOMPUTE_EXTRINSIC
        + cv2.fisheye.CALIB_FIX_SKEW
        + cv2.fisheye.CALIB_CHECK_COND
    )

    criteria = (
        cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER,
        200,
        1e-7,
    )

    try:
        rms, K, D, rvecs, tvecs = cv2.fisheye.calibrate(
            object_points_fisheye,
            image_points_fisheye,
            image_size,
            K,
            D,
            None,
            None,
            flags,
            criteria,
        )

        return rms, K, D, rvecs, tvecs

    except cv2.error as exc:
        print("\nInitial fisheye calibration failed.")
        print("Trying to identify and remove bad samples...")
        print(f"\nOriginal OpenCV error:\n{exc}\n")

    good_object_points = []
    good_image_points = []
    bad_indices = []

    for i, (obj, img) in enumerate(
        zip(object_points_fisheye, image_points_fisheye)
    ):
        test_K = K.copy()
        test_D = np.zeros((4, 1), dtype=np.float64)

        try:
            cv2.fisheye.calibrate(
                [obj],
                [img],
                image_size,
                test_K,
                test_D,
                None,
                None,
                flags,
                criteria,
            )

            good_object_points.append(obj)
            good_image_points.append(img)

        except cv2.error:
            bad_indices.append(i)

    if bad_indices:
        print("Rejected fisheye-unfriendly samples:")
        for index in bad_indices:
            print(f"  Sample {index + 1}")

    if len(good_object_points) < 5:
        raise RuntimeError(
            "Too few usable samples for fisheye calibration after rejecting "
            f"bad ones. Usable samples: {len(good_object_points)}"
        )

    print(
        f"Retrying fisheye calibration with "
        f"{len(good_object_points)}/{len(object_points_fisheye)} samples..."
    )

    K = np.array(
        [
            [focal_guess, 0.0, width / 2.0],
            [0.0, focal_guess, height / 2.0],
            [0.0, 0.0, 1.0],
        ],
        dtype=np.float64,
    )

    D = np.zeros((4, 1), dtype=np.float64)

    rms, K, D, rvecs, tvecs = cv2.fisheye.calibrate(
        good_object_points,
        good_image_points,
        image_size,
        K,
        D,
        None,
        None,
        flags,
        criteria,
    )

    return rms, K, D, rvecs, tvecs

def make_calibration_set_dir(args):
    if args.no_save_calibration_set:
        return None

    if args.load_calibration_set:
        return None

    if args.calibration_set_dir:
        run_dir = Path(args.calibration_set_dir)
    else:
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        run_dir = Path("calibration_sets") / f"calibration_{timestamp}"

    run_dir.mkdir(parents=True, exist_ok=True)

    print(f"\nSaving calibration set to:")
    print(f"  {run_dir.resolve()}")

    return run_dir


def save_calibration_set(
    run_dir,
    object_points,
    image_points,
    image_size,
    checkerboard_size,
    square_size,
    args,
):
    if run_dir is None:
        return

    points_path = run_dir / "calibration_points.npz"
    metadata_path = run_dir / "metadata.json"

    np.savez_compressed(
        points_path,
        object_points=np.asarray(object_points, dtype=np.float32),
        image_points=np.asarray(image_points, dtype=np.float32),
        image_size=np.asarray(image_size, dtype=np.int32),
        checkerboard_size=np.asarray(checkerboard_size, dtype=np.int32),
        square_size=np.asarray([square_size], dtype=np.float64),
    )

    metadata = {
        "image_size": {
            "width": int(image_size[0]),
            "height": int(image_size[1]),
        },
        "checkerboard_size": {
            "width_inner_corners": int(checkerboard_size[0]),
            "height_inner_corners": int(checkerboard_size[1]),
        },
        "square_size": float(square_size),
        "samples": int(len(object_points)),
        "source": str(args.source),
        "source_type": str(args.source_type),
        "resolution": args.resolution,
        "saved_at_unix_time": time.time(),
    }

    with open(metadata_path, "w", encoding="utf-8") as file:
        json.dump(metadata, file, indent=2)

    latest_path = run_dir.parent / "latest_calibration_set.txt"
    latest_path.parent.mkdir(parents=True, exist_ok=True)

    with open(latest_path, "w", encoding="utf-8") as file:
        file.write(str(run_dir.resolve()))


def save_accepted_frame(run_dir, frame, display, sample_index):
    if run_dir is None:
        return

    raw_path = run_dir / f"sample_{sample_index:03d}.png"
    preview_path = run_dir / f"sample_{sample_index:03d}_preview.png"

    cv2.imwrite(str(raw_path), frame)
    cv2.imwrite(str(preview_path), display)


def load_calibration_set(load_dir):
    load_dir = Path(load_dir)
    points_path = load_dir / "calibration_points.npz"

    if not points_path.exists():
        raise FileNotFoundError(
            f"Could not find saved calibration points: {points_path}"
        )

    data = np.load(points_path, allow_pickle=False)

    object_points = [
        obj.astype(np.float32)
        for obj in data["object_points"]
    ]

    image_points = [
        img.astype(np.float32)
        for img in data["image_points"]
    ]

    image_size = tuple(int(value) for value in data["image_size"])
    checkerboard_size = tuple(
        int(value) for value in data["checkerboard_size"]
    )
    square_size = float(data["square_size"][0])

    print("\nLoaded saved calibration set:")
    print(f"  Directory: {load_dir.resolve()}")
    print(f"  Samples: {len(object_points)}")
    print(f"  Image size: {image_size[0]} x {image_size[1]}")
    print(f"  Checkerboard inner corners: {checkerboard_size}")
    print(f"  Square size: {square_size}")

    return object_points, image_points, image_size, checkerboard_size, square_size

def main():
    args = parse_args()

    calibration_set_dir = make_calibration_set_dir(args)

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

    if args.load_calibration_set:
        (
            object_points,
            image_points,
            image_size,
            checkerboard_size,
            square_size,
        ) = load_calibration_set(args.load_calibration_set)

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

    if not args.load_calibration_set:
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

                    sample_index = len(object_points)

                    save_accepted_frame(
                        calibration_set_dir,
                        frame,
                        display,
                        sample_index,
                    )

                    save_calibration_set(
                        calibration_set_dir,
                        object_points,
                        image_points,
                        image_size,
                        checkerboard_size,
                        square_size,
                        args,
                    )

                    print(
                        f"Accepted sample {sample_index}/{args.samples}"
                    )

                    if calibration_set_dir is not None:
                        print(
                            "Saved calibration set checkpoint to "
                            f"{calibration_set_dir}"
                        )
                else:
                    print("Cannot accept sample: checkerboard not found.")

        cv2.destroyWindow("Calibration")

    if len(object_points) < 3:
        print("ERROR: Not enough samples for calibration.")
        sys.exit(1)

    print("\nCalibrating camera using both models...")

    print("\nRunning pinhole calibration...")
    pinhole_rms, pinhole_K, pinhole_D, pinhole_rvecs, pinhole_tvecs = (
        calibrate_pinhole(
            object_points,
            image_points,
            image_size,
        )
    )

    print_pinhole_calibration_results(
        pinhole_K,
        pinhole_D,
        pinhole_rms,
        image_size,
    )

    print("\nRunning fisheye calibration...")
    # fisheye_rms, fisheye_K, fisheye_D, fisheye_rvecs, fisheye_tvecs = (
    #     calibrate_fisheye(
    #         object_points,
    #         image_points,
    #         image_size,
    #     )
    # )

    # print_fisheye_calibration_results(
    #     fisheye_K,
    #     fisheye_D,
    #     fisheye_rms,
    #     image_size,
    # )

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

            if True:
                new_K, roi = cv2.getOptimalNewCameraMatrix(
                    pinhole_K,
                    pinhole_D,
                    (w, h),
                    1,
                    (w, h),
                )

                undistorted = cv2.undistort(
                    frame,
                    pinhole_K,
                    pinhole_D,
                    None,
                    new_K,
                )
            if False:
                new_K = cv2.fisheye.estimateNewCameraMatrixForUndistortRectify(
                    fisheye_K,
                    fisheye_D,
                    (w, h),
                    np.eye(3),
                    balance=0.0,
                )

                map1, map2 = cv2.fisheye.initUndistortRectifyMap(
                    fisheye_K,
                    fisheye_D,
                    np.eye(3),
                    new_K,
                    (w, h),
                    cv2.CV_16SC2,
                )

                undistorted = cv2.remap(
                    frame,
                    map1,
                    map2,
                    interpolation=cv2.INTER_LINEAR,
                    borderMode=cv2.BORDER_CONSTANT,
                )

            cv2.imshow("Original", frame)
            cv2.imshow("Undistorted", undistorted)

            key = cv2.waitKey(1) & 0xFF

            if key in [ord("q"), 27]:
                break

    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()

import os

os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = "rtsp_transport;tcp|max_delay;0"

import sys
import argparse
import glob
import time
import platform

import cv2
import numpy as np
from ultralytics import YOLO

import subprocess
import threading

def parse_args():
    parser = argparse.ArgumentParser(description="Scan for invasive crabs with BlueStar")

    parser.add_argument(
        "--model-path",
        default="Standard_Model.pt",
        help="Path to the Yolo model used"
    )

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
        "--min-thresh",
        default=0.65,
        help="Minimum threshhold for marking as detected",
        type=float
    )

    parser.add_argument(
        "--resolution",
        default="1280x720",
        help="Source resolution (required for FFmpeg backend)"
    )

    parser.add_argument(
        "--device",
        default="mps",
        help="Inference device: cpu, mps (macOS only), or auto"
    )

    parser.add_argument(
        "--imgsz",
        default=640,
        type=int,
        help="YOLO inference image size"
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

    return parser.parse_args()

class OpenCVFrameSource:
    def __init__(self, source, source_type, width=None, height=None):
        if source_type == "usb":
            cap_arg = int(source[3:])
        else:
            cap_arg = source

        self.cap = cv2.VideoCapture(cap_arg)

        if width is not None and height is not None:
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)

    def read(self):
        return self.cap.read()

    def release(self):
        self.cap.release()


class FFmpegLatestFrameSource:
    def __init__(
        self,
        url,
        width,
        height,
        rtsp_transport="tcp",
        loglevel="error",
        use_videotoolbox=True,
    ):
        self.url = url
        self.width = width
        self.height = height
        self.frame_size = width * height * 3
        self.rtsp_transport = rtsp_transport
        self.loglevel = loglevel
        self.use_videotoolbox = use_videotoolbox

        self.proc = None
        self.thread = None
        self.running = False
        self.lock = threading.Lock()
        self.frame = None

    def start(self):
        cmd = [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            self.loglevel,
        ]

        if self.url.lower().startswith("rtsp://"):
            cmd += [
                "-rtsp_transport",
                self.rtsp_transport,
                "-fflags",
                "nobuffer",
                "-flags",
                "low_delay",
            ]

        if self.use_videotoolbox:
            cmd += [
                "-hwaccel",
                "videotoolbox",
            ]

        cmd += [
            "-i",
            self.url,
            "-an",
            "-vf",
            f"scale={self.width}:{self.height}",
            "-pix_fmt",
            "bgr24",
            "-f",
            "rawvideo",
            "pipe:1",
        ]

        self.proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            bufsize=self.frame_size * 4,
        )

        self.running = True
        self.thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.thread.start()

    def _read_exact(self, size):
        chunks = []
        remaining = size

        while remaining > 0 and self.running:
            chunk = self.proc.stdout.read(remaining)

            if not chunk:
                return None

            chunks.append(chunk)
            remaining -= len(chunk)

        return b"".join(chunks)

    def _reader_loop(self):
        while self.running:
            raw = self._read_exact(self.frame_size)

            if raw is None:
                self.running = False
                break

            frame = np.frombuffer(raw, dtype=np.uint8)
            frame = frame.reshape((self.height, self.width, 3))

            with self.lock:
                self.frame = frame.copy()

    def read(self):
        while self.running:
            with self.lock:
                if self.frame is not None:
                    return True, self.frame.copy()

            time.sleep(0.005)

        return False, None

    def release(self):
        self.running = False

        if self.proc is not None:
            self.proc.terminate()

            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()

args = parse_args()

# Check if model file exists and is valid
if (not os.path.exists(args.model_path)):
    print('ERROR: Model path is invalid or model was not found. Make sure the model filename was entered correctly.')
    sys.exit(0)

# Load the model into memory and get labemap
model = YOLO(args.model_path, task='detect')
labels = model.names

# Parse user-specified display resolution
resize = False
if args.resolution:
    resize = True
    resW, resH = int(args.resolution.split('x')[0]), int(args.resolution.split('x')[1])

# Load or initialize image source
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

        frame_source = FFmpegLatestFrameSource(
            url=args.source,
            width=resW,
            height=resH,
            loglevel=args.ffmpeg_loglevel,
            use_videotoolbox=platform.system() == "Darwin",
        )
        frame_source.start()

    else:
        frame_source = OpenCVFrameSource(
            source=args.source,
            source_type=args.source_type,
            width=resW if args.resolution else None,
            height=resH if args.resolution else None,
        )

# Set bounding box colors (using the Tableu 10 color scheme)
bbox_colors = [(164,120,87), (68,148,228), (93,97,209), (178,182,133), (88,159,106), 
              (96,202,231), (159,124,168), (169,162,241), (98,118,150), (172,176,184)]

# Initialize control and status variables
avg_frame_rate = 0
frame_rate_buffer = []
fps_avg_len = 200
img_count = 0

# Begin inference loop
while True:
    t_start = time.perf_counter()

    # Load frame from image source
    ret, frame = frame_source.read()

    if not ret or frame is None:
        print("Unable to read frame from source. Exiting program.")
        break

    # Resize frame to desired display resolution
    if resize and args.capture_backend != "ffmpeg":
        frame = cv2.resize(frame, (resW, resH))

    # Run inference on frame
    is_coreml_model = args.model_path.endswith((".mlpackage", ".mlmodel"))

    predict_kwargs = {
        "source": frame,
        "imgsz": args.imgsz,
        "conf": args.min_thresh,
        "verbose": False,
    }

    if not is_coreml_model:
        predict_kwargs["device"] = args.device

    results = model.predict(**predict_kwargs)

    # Extract results
    detections = results[0].boxes

    # Initialize variable for basic object counting example
    object_count = 0

    # Go through each detection and get bbox coords, confidence, and class
    for i in range(len(detections)):

        # Get bounding box coordinates
        # Ultralytics returns results in Tensor format, which have to be converted to a regular Python array
        xyxy_tensor = detections[i].xyxy.cpu() # Detections in Tensor format in CPU memory
        xyxy = xyxy_tensor.numpy().squeeze() # Convert tensors to Numpy array
        xmin, ymin, xmax, ymax = xyxy.astype(int) # Extract individual coordinates and convert to int

        # Get bounding box class ID and name
        classidx = int(detections[i].cls.item())
        classname = labels[classidx]

        # Get bounding box confidence
        conf = detections[i].conf.item()

        # Draw box if confidence threshold is high enough
        if conf > args.min_thresh:

            color = bbox_colors[classidx % 10]
            cv2.rectangle(frame, (xmin,ymin), (xmax,ymax), color, 2)

            label = f'{classname}: {int(conf*100)}%'
            labelSize, baseLine = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1) # Get font size
            label_ymin = max(ymin, labelSize[1] + 10) # Make sure not to draw label too close to top of window
            cv2.rectangle(frame, (xmin, label_ymin-labelSize[1]-10), (xmin+labelSize[0], label_ymin+baseLine-10), color, cv2.FILLED) # Draw white box to put label text in
            cv2.putText(frame, label, (xmin, label_ymin-7), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 1) # Draw label text

            # Basic example: count the number of objects in the image
            object_count = object_count + 1

    # Calculate and draw framerate (if using video, USB, or Picamera source)
    if args.source_type == 'video' or args.source_type == 'usb':
        cv2.putText(frame, f'FPS: {avg_frame_rate:0.2f}', (10,20), cv2.FONT_HERSHEY_SIMPLEX, .7, (0,255,255), 2) # Draw framerate
    
    # Display detection results
    cv2.putText(frame, f'Number of objects: {object_count}', (10,45), cv2.FONT_HERSHEY_SIMPLEX, .7, (0,255,255), 2) # Draw total number of detected objects

    cv2.putText(frame, f'Threshold: {args.min_thresh:.2f}', (10,70), cv2.FONT_HERSHEY_SIMPLEX, .7, (0,255,255), 2) # Draw current detection threshold
    cv2.imshow('YOLO detection results',frame) # Display image

    # Wait 5ms before moving to next frame.
    if args.source_type == 'video' or args.source_type == 'usb':
        key = cv2.waitKey(5)
    
    if key == ord('q') or key == ord('Q'): # Press 'q' to quit
        break
    elif key == ord('1'): # Press '1' to pause inference
        cv2.waitKey()
    elif key == ord('-') or key == ord('_'): # Press '-/_' to lower the detection threshold
        if args.min_thresh > 0.1:
            args.min_thresh -= 0.05
    elif key == ord('=') or key == ord('+'): # Press '=/+' to increase the detection threshold
        if args.min_thresh < 0.90:
            args.min_thresh += 0.05
    elif key == ord('2'): # Press '2' to save a picture of results on this frame
        cv2.imwrite('capture.png',frame)
    
    # Calculate FPS for this frame
    t_stop = time.perf_counter()
    frame_rate_calc = float(1/(t_stop - t_start))

    # Append FPS result to frame_rate_buffer (for finding average FPS over multiple frames)
    if len(frame_rate_buffer) >= fps_avg_len:
        temp = frame_rate_buffer.pop(0)
        frame_rate_buffer.append(frame_rate_calc)
    else:
        frame_rate_buffer.append(frame_rate_calc)

    # Calculate average FPS for past frames
    avg_frame_rate = np.mean(frame_rate_buffer)

# Clean up
print(f'Average pipeline FPS: {avg_frame_rate:.2f}')
if frame_source is not None:
    frame_source.release()
cv2.destroyAllWindows()
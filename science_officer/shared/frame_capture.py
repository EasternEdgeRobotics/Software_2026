import cv2
from math import sqrt
import os
import subprocess
import threading
import numpy as np
import time
import platform

from dataclasses import dataclass
from typing import Optional

@dataclass(frozen=True)
class FrameSourceConfig:
    source: str
    source_type: str
    capture_backend: str = "opencv"
    width: Optional[int] = None
    height: Optional[int] = None
    ffmpeg_loglevel: str = "warning"
    no_signal_image_path: Optional[str] = None

    @classmethod
    def from_args(cls, args):
        width = None
        height = None

        if getattr(args, "resolution", None):
            width, height = parse_resolution(args.resolution)

        return cls(
            source=args.source,
            source_type=args.source_type,
            capture_backend=args.capture_backend,
            width=width,
            height=height,
            ffmpeg_loglevel=getattr(args, "ffmpeg_loglevel", "warning"),
            no_signal_image_path=getattr(args, "no_signal_image_path", None),
        )


def parse_resolution(resolution: str) -> tuple[int, int]:
    try:
        width_str, height_str = resolution.lower().split("x", 1)
        return int(width_str), int(height_str)
    except ValueError as exc:
        raise ValueError(
            "Resolution must be in WIDTHxHEIGHT format, for example 1920x1080."
        ) from exc
    
def create_frame_source(config: FrameSourceConfig):
    if config.source_type not in {"video", "usb"}:
        raise ValueError(
            f"Unsupported source type: {config.source_type!r}. "
            "Expected 'video' or 'usb'."
        )

    if config.capture_backend == "ffmpeg":
        return _create_ffmpeg_frame_source(config)

    if config.capture_backend == "opencv":
        return _create_opencv_frame_source(config)

    raise ValueError(
        f"Unsupported capture backend: {config.capture_backend!r}. "
        "Expected 'opencv' or 'ffmpeg'."
    )


def _create_ffmpeg_frame_source(config: FrameSourceConfig):
    if config.source_type == "usb":
        raise ValueError(
            "FFmpeg backend is intended for video/RTSP sources. "
            "Use capture_backend='opencv' for USB cameras."
        )

    if config.width is None or config.height is None:
        raise ValueError(
            "FFmpeg backend requires a resolution in WIDTHxHEIGHT format."
        )

    return FFmpegFrameSource(
        url=config.source,
        width=config.width,
        height=config.height,
        loglevel=config.ffmpeg_loglevel,
        no_signal_image_path=config.no_signal_image_path,
    )


def _create_opencv_frame_source(config: FrameSourceConfig):
    return OpenCVFrameSource(
        source=config.source,
        source_type=config.source_type,
        width=config.width,
        height=config.height,
    )

class FFmpegFrameSource:
    def __init__(
        self,
        url,
        width,
        height,
        rtsp_transport="tcp",
        loglevel="error",
        no_signal_image_path=None,
        retry_delay=3.0,
    ):
        self.url = url
        self.width = width
        self.height = height
        self.frame_size = width * height * 3
        self.rtsp_transport = rtsp_transport
        self.loglevel = loglevel
        self.retry_delay = retry_delay
        self.use_videotoolbox = platform.system() == "Darwin"

        self.proc = None
        self.thread = None
        self.running = False
        self.lock = threading.Lock()

        self.no_signal_frame = cv2.imread(no_signal_image_path)
        self.no_signal_frame = cv2.resize(
            self.no_signal_frame,
            (self.width, self.height),
        )

        self.frame = self.no_signal_frame.copy()

    def start(self):
        self.running = True
        self.thread = threading.Thread(target=self._connection_loop, daemon=True)
        self.thread.start()

    def _build_cmd(self):
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

        return cmd

    def _connection_loop(self):
        while self.running:
            with self.lock:
                self.frame = self.no_signal_frame.copy()

            cmd = self._build_cmd()

            self.proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                bufsize=self.frame_size * 4,
            )

            while self.running:
                raw = self._read_exact(self.frame_size)

                if raw is None:
                    break

                frame = np.frombuffer(raw, dtype=np.uint8)
                frame = frame.reshape((self.height, self.width, 3))

                with self.lock:
                    self.frame = frame.copy()

            self._stop_ffmpeg()

            if self.running:
                print(f"FFmpeg stream disconnected. Retrying in {self.retry_delay} seconds...")
                time.sleep(self.retry_delay)

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

    def read(self):
        if not self.running:
            return False, None

        with self.lock:
            return True, self.frame.copy()

    def _stop_ffmpeg(self):
        if self.proc is None:
            return

        self.proc.terminate()

        try:
            self.proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()

        self.proc = None

    def release(self):
        self.running = False
        self._stop_ffmpeg()

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

    def start(self): # I want to keep parity between backends
        return 
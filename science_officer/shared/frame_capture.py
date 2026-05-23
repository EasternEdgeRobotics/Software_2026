import cv2
from math import sqrt
import os
import subprocess
import threading
import numpy as np
import time
import platform

class FFmpegFrameSource:
    def __init__(
        self,
        url,
        width,
        height,
        rtsp_transport="tcp",
        loglevel="error"
    ):
        self.url = url
        self.width = width
        self.height = height
        self.frame_size = width * height * 3
        self.rtsp_transport = rtsp_transport
        self.loglevel = loglevel
        self.use_videotoolbox = platform.system() == "Darwin"

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
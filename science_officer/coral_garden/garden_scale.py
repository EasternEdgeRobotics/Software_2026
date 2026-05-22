import cv2
from math import sqrt
import os
import argparse
import subprocess
import threading
import numpy as np
import time
import platform
import sys

os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = "rtsp_transport;tcp|max_delay;0"

def parse_args():
    parser = argparse.ArgumentParser(description="Measure the coral garden scale with BlueStar")

    parser.add_argument(
        "--source-type",
        default="video",
        help="video or usb"
    )

    parser.add_argument(
        "--source",
        default="rtsp://192.168.137.200:8889/cam2",
        help="Video source (ie usb0, rtsp://192.168.137.200:8889/cam2)"
    )

    parser.add_argument(
        "--resolution",
        default="1260x720",
        help="Source resolution"
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

def text_with_background(img, text, position, font=cv2.FONT_HERSHEY_SIMPLEX, font_scale=1, text_color=(255, 255, 255), bg_color=(0, 0, 0), thickness=2, padding=5,):
    x, y = position

    text_size, baseline = cv2.getTextSize(text, font, font_scale, thickness)
    text_width, text_height = text_size

    top_left = (x - padding, y - text_height - padding)
    bottom_right = (x + text_width + padding, y + baseline + padding)

    cv2.rectangle(img, top_left, bottom_right, bg_color, cv2.FILLED)

    cv2.putText(img, text, position, font, font_scale, text_color, thickness, cv2.LINE_AA,)

def points(event,x,y,flags,param):
        clicked_points = param
        global mouse_x, mouse_y
        if event == cv2.EVENT_MOUSEMOVE:
            mouse_x, mouse_y = x,y
            #print(f"Mouse Position: ({mouse_x},{mouse_y})")

        if event == cv2.EVENT_LBUTTONDOWN:
            if len(clicked_points) == 6:
                clicked_points.clear()
            #cv2.circle(img2,(x,y),5,(255,255,0),-1)
            clicked_points.append((x,y))
            #print(f"Point added: ({x}, {y})")
        
def line_distance(p1,p2):
        x_dif = p2[0]-p1[0]
        y_dif = p2[1]-p1[1]
        distance = sqrt(x_dif**2 + y_dif**2)
        #print(distance,p1,p2)
        return distance


def cam_mode():
        heights = []
        clicked_points = []
        auto = True
        while True:
            # Capture frame-by-frame
            ret, frame = frame_source.read()

            #Nothing was returned
            if not ret:
                print("Error: Failed to capture frame.")
                break
            
            #Renaming variable
            global img1
            img1 = frame[:]
            #Honestly forget what this does
            #i think it checks for the last two bytes to indicate letter "q" so press q
            #if in loop it would probably be useful for screenshotting if wanted
            global mode
            global key
        
            key = cv2.waitKey(1)
            if key & 0xFF == ord('2'):
                
                auto = False
                print("auto is false")
                photo = img1.copy()

            if key == ord('q') or key == ord('Q'):
                mode = 3
                break

            if auto:
            
                draw_mode(img1,heights,clicked_points)
            
            else: 
                
                draw_mode(photo,heights,clicked_points)
            
        return heights
    
def draw_mode(picture,heights, clicked_points):

        imgconst = picture.copy()
        global rheight
        rheight = 0
        
        img2 = imgconst.copy()
        
        #print(f"Mouse Position: ({mouse_x},{mouse_y})")
    
        if mouse_x != -1:
            cv2.circle(img2,(mouse_x,mouse_y),10,(50,50,255),-1)
        for point in clicked_points:
            cv2.circle(img2,point,10,(50,0,255),-1)
        if len(clicked_points) > 1:
            cv2.line(img2, clicked_points[0], clicked_points[1],(255,0,0),5)
        if len(clicked_points) == 6:
            cv2.line(img2, clicked_points[2], clicked_points[3],(0,255,0),5)
            cv2.line(img2, clicked_points[4], clicked_points[5],(0,0,255),5)
            ref_width_pxdistance = line_distance(clicked_points[0],clicked_points[1])
            height_pxdistance = line_distance(clicked_points[2],clicked_points[3])
            width_pxdistance = line_distance(clicked_points[4],clicked_points[5])
            
            ref_width = 45
            if ref_width != 0 and ref_width_pxdistance != 0 and width_pxdistance != 0 and height_pxdistance != 0:
                rheight = ref_width/ref_width_pxdistance*height_pxdistance
                rheight = ref_width/ref_width_pxdistance*height_pxdistance
                rwidth = ref_width/ref_width_pxdistance*width_pxdistance
                rwidth = ref_width/ref_width_pxdistance*width_pxdistance
                rheight = round(rheight,2)
                rwidth = round(rwidth,2)
                text_with_background(img2, f"{ref_width}cm", clicked_points[0])
                text_with_background(img2, f"{rheight}cm", clicked_points[3])
                text_with_background(img2, f"{rwidth}cm", clicked_points[5])
                
            else:
                 print(f"Invalid Points!!! {ref_width} {width_pxdistance} {height_pxdistance}")
            
        cv2.imshow("Coral Garden Measurement", img2 )
        cv2.setMouseCallback('Coral Garden Measurement', points, param = clicked_points)

        if key & 0xFF == ord('1'):
            if rheight not in heights and rheight != 0:
                heights.append(rheight)
                print("New Height: ", rheight)

def main():
    global mouse_x,mouse_y
    mouse_x,mouse_y = -1,-1

    global mode
    mode = 1
    clicked_points = []
    
    while True:
        if mode == 3:
            #quit
            if frame_source is not None:
                frame_source.release()
            cv2.destroyAllWindows()

            if len(heights) != 0:
                avg = sum(heights)/len(heights)
                avg = round(avg,2)
                print(f"Average Depth {avg}")

            break
        elif mode == 1:
            heights = cam_mode()
            if len(heights) != 0:
                print(heights)

main()
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
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = "rtsp_transport;tcp|max_delay;0"

from shared import frame_capture
from shared import opencv_helpers
from shared import fisheye_list
from shared import common_args

def parse_args():
    parser = argparse.ArgumentParser(description="Measure the coral garden scale with BlueStar")

    common_args.video_args(parser)

    parser.add_argument(
        "--fisheye-correction",
        default=False,
        action="store_true",
        help="Enable Fisheye correction",
    )

    return parser.parse_args()

args = parse_args()

# Parse user-specified display resolution
resize = False
if args.resolution:
    resize = True
    resW, resH = int(args.resolution.split('x')[0]), int(args.resolution.split('x')[1])

# Load certain fisheye correction profiles based off the source
K, D = fisheye_list.get_correction(args.source, resW, resH)

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

        frame_source = frame_capture.FFmpegFrameSource(
            url=args.source,
            width=resW,
            height=resH,
            loglevel=args.ffmpeg_loglevel
        )
        frame_source.start()

    else:
        frame_source = frame_capture.FFmpegFrameSource(
            source=args.source,
            source_type=args.source_type,
            width=resW if args.resolution else None,
            height=resH if args.resolution else None,
        )



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

        if args.fisheye_correction == True:
            balance = 0.3
            map1, map2 = opencv_helpers.prepare_fisheye(K, D, (int(args.resolution.split('x')[0]), int(args.resolution.split('x')[1])), balance)

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

            if args.fisheye_correction == True:
                img1 = cv2.remap(img1, map1, map2, interpolation=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT)

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
            opencv_helpers.draw_zoom_cursor(img2, imgconst, (mouse_x, mouse_y), zoom=2.5, lens_radius=90)
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
                opencv_helpers.text_with_background(img2, f"{ref_width}cm", clicked_points[0])
                opencv_helpers.text_with_background(img2, f"{rheight}cm", clicked_points[3])
                opencv_helpers.text_with_background(img2, f"{rwidth}cm", clicked_points[5])
                
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
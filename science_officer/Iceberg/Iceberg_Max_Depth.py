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
from dataclasses import replace

PROJECT_ROOT = Path(__file__).resolve().parents[2]
OFFICER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(OFFICER_ROOT))

os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = "rtsp_transport;tcp|max_delay;0"

from shared import frame_capture
from shared import opencv_helpers
from shared import fisheye_list
from shared import common_args

FISHEYE_INVALID = False
PINHOLE_INVALID = False

TOP_PIPE_REF_CM = 64.0
KNOWN_VERTICAL_REF_CM = 15.0

def parse_args():
    parser = argparse.ArgumentParser(description="Measure icebergs with BlueStar")

    common_args.video_args(parser)
    common_args.fisheye_args(parser)
    common_args.measurement_args(parser)

    parser.add_argument(
        "--disable-vertical-pole",
        default=False,
        action="store_true",
        help="Disable using the vertical pole for measurement refrence",
    )

    parser.add_argument(
        "--enable-grid",
        default=False,
        action="store_true",
        help="Enable grid for measurement",
    )

    return parser.parse_args()

args = parse_args()

# Initialize image source
try:
    config = frame_capture.FrameSourceConfig.from_args(args)
    config = replace(
        config,
        no_signal_image_path=str(PROJECT_ROOT / "Assets" / "nosignal_dark.jpg"),
    )
    frame_source = frame_capture.create_frame_source(config)
    frame_source.start()
except ValueError as exc:
    print(f"ERROR: {exc}")
    sys.exit(1)

# Parse user-specified display resolution
if args.resolution:
    resW, resH = frame_capture.parse_resolution(args.resolution)

# Load certain fisheye correction profiles based off the source
K_FISHEYE, D_FISHEYE, K_PINHOLE, D_PINHOLE = fisheye_list.get_correction(args.source, resW, resH)

if K_PINHOLE is None and D_PINHOLE is None:
    PINHOLE_INVALID = True
    print(f"No pinhole correction maps availble for: {args.source}")
if K_FISHEYE is None and D_FISHEYE is None:
    FISHEYE_INVALID = True
    print(f"No fisheye correction maps availble for: {args.source}")

def points(event, x, y, flags, param):
        clicked_points = param
        global mouse_x, mouse_y
        if event == cv2.EVENT_MOUSEMOVE:
            mouse_x, mouse_y = x,y
            #print(f"Mouse Position: ({mouse_x},{mouse_y})")

        if event == cv2.EVENT_LBUTTONDOWN:
            if len(clicked_points) == 4:
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
        freeze = False

        if args.fisheye_type == "pinhole" and PINHOLE_INVALID == False:
            map1, map2 = opencv_helpers.prepare_pinhole(K_PINHOLE, D_PINHOLE, (int(args.resolution.split('x')[0]), int(args.resolution.split('x')[1])))
        elif args.fisheye_type == "fisheye" and FISHEYE_INVALID == False:
            balance = 0.3
            map1, map2 = opencv_helpers.prepare_fisheye(K_FISHEYE, D_FISHEYE, (int(args.resolution.split('x')[0]), int(args.resolution.split('x')[1])), balance)

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
                if args.fisheye_type == "pinhole" and PINHOLE_INVALID == False: # I dont think this is actually needed, but i cant test it so im not risking it. -PC
                    img1 = cv2.remap(img1, map1, map2, interpolation=cv2.INTER_LINEAR)
                elif args.fisheye_type == "fisheye" and FISHEYE_INVALID == False:
                    img1 = cv2.remap(img1, map1, map2, interpolation=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT)

            #Honestly forget what this does
            #i think it checks for the last two bytes to indicate letter "q" so press q
            #if in loop it would probably be useful for screenshotting if wanted
            global mode
            global key
        
            key = cv2.waitKey(1)
            if key & 0xFF == ord('2'):
                freeze = not freeze
                if freeze:
                    print("Frozen")
                    photo = img1.copy()
            
            if key == ord('q') or key == ord('Q'):
                mode = 3
                break

            if key == ord('3'): # Reset points
                clicked_points = []
            
            if key == ord('4'): # Toggle Fisheye correction
                args.fisheye_correction = not args.fisheye_correction

            if key == ord('5'): # Toggle vertical pole reference 
                args.disable_vertical_pole = not args.disable_vertical_pole
                clicked_points = []

            if key == ord('6'):
                args.enable_grid = not args.enable_grid

            if not freeze:
                clicked_points = draw_mode(img1,heights,clicked_points)
            else: 
                clicked_points = draw_mode(photo,heights,clicked_points)
            
        return heights

def draw_mode(picture,heights, clicked_points):
        global rheight, TOP_PIPE_REF_CM
        rheight = 0

        img2 = picture.copy()
                
        #print(f"Mouse Position: ({mouse_x},{mouse_y})")
        point_colours = [
            (0, 255, 0),
            (0, 255, 0),
            (255, 0, 0),
            (255, 0, 0),
        ]
        
        for i, point in enumerate(clicked_points):
            cv2.circle(img2, point, 10, point_colours[i], -1)

        if args.enable_grid == True:
            # Thirds
            resW_intv = int(round(resW / 3, 0))
            resH_intv = int(round(resH / 3, 0))

            cv2.line(img2, (resW_intv, 0), (resW_intv * 1, int(resW)), (0, 0, 255), 2,)
            cv2.line(img2, (resW_intv * 2, 0), (resW_intv * 2, int(resW)), (0, 0, 255), 2,)

            cv2.line(img2, (0, resH_intv * 1), (int(resW), resH_intv * 1), (0, 0, 255), 2,)
            cv2.line(img2, (0, resH_intv * 2), (int(resW), resH_intv * 2), (0, 0, 255), 2,)

        if args.follow_mouse == True:
            overlay = img2.copy()

            if len(clicked_points) == 1: 
                cv2.line(overlay, clicked_points[0], (mouse_x, mouse_y), point_colours[0], 5,)
            elif len(clicked_points) == 2 and args.disable_vertical_pole == False:
                cv2.line(overlay, clicked_points[1], (mouse_x, mouse_y), point_colours[1], 5,)
            elif len(clicked_points) == 3: 
                cv2.line(overlay, clicked_points[2], (mouse_x, mouse_y), point_colours[2], 5,)
            
            alpha = 0.4
            img2 = cv2.addWeighted(overlay, alpha, img2, 1 - alpha, 0)

        if args.disable_vertical_pole == False:
            if len(clicked_points) >= 2: # Known 15 cm pole: point 1 to point 2
                cv2.line(img2, clicked_points[0], clicked_points[1], (0, 255, 0), 5,)

            if len(clicked_points) >= 3: # Top 60 cm pipe: point 2 to point 3
                cv2.line(img2, clicked_points[1], clicked_points[2], (0, 0, 255), 5,)

            if len(clicked_points) >= 4: # Unknown variable pole: point 3 to point 4
                cv2.line(img2, clicked_points[2], clicked_points[3], (255, 0, 0), 5,)
            
                known_pole_px = line_distance(clicked_points[0], clicked_points[1])
                top_pipe_px = line_distance(clicked_points[1], clicked_points[2])
                unknown_pole_px = line_distance(clicked_points[2], clicked_points[3])

                if known_pole_px != 0 and top_pipe_px != 0 and unknown_pole_px != 0:
                    vertical_cm_per_px = KNOWN_VERTICAL_REF_CM / known_pole_px
                    horizontal_cm_per_px = TOP_PIPE_REF_CM / top_pipe_px

                    avg_known_cm_per_px = sum((vertical_cm_per_px, horizontal_cm_per_px)) / len((vertical_cm_per_px, horizontal_cm_per_px))

                    unknown_from_vertical_ref_cm = unknown_pole_px * avg_known_cm_per_px

                    rheight = round(unknown_from_vertical_ref_cm, 2)

                    opencv_helpers.text_with_background(img2, f"Top Ref: {TOP_PIPE_REF_CM}cm", (10,30))
                    opencv_helpers.text_with_background(img2, f"Pole Ref: {KNOWN_VERTICAL_REF_CM}cm", (10,70))
                    opencv_helpers.text_with_background(img2, f"Length: {rheight}cm", (10,110))

                else:
                    print("Invalid points:", known_pole_px, top_pipe_px, unknown_pole_px,)
                    clicked_points = []


        else:
            if len(clicked_points) >= 2:
                cv2.line(img2, clicked_points[0], clicked_points[1],(0,0,255),5)
            if len(clicked_points) == 4:
                cv2.line(img2, clicked_points[2], clicked_points[3],(255,0,0),5)
                width_pxdistance = line_distance(clicked_points[0],clicked_points[1])
                height_pxdistance = line_distance(clicked_points[2],clicked_points[3])
                
                rwidth = TOP_PIPE_REF_CM
                if rwidth != 0 and width_pxdistance != 0 and height_pxdistance != 0:
                    rheight = rwidth/width_pxdistance*height_pxdistance
                    rheight = round(rheight,2)
                    opencv_helpers.text_with_background(img2, f"Ref: {rwidth}cm", (10,30))
                    opencv_helpers.text_with_background(img2, f"Length: {rheight}cm", (10,70))
                    
                else:
                    print(f"Invalid Points!!! {rwidth} {width_pxdistance} {height_pxdistance}")
                    clicked_points = []

        if mouse_x != -1:
            opencv_helpers.draw_zoom_cursor(img2, img2, (mouse_x, mouse_y), zoom=3.0, lens_radius=150)
            
        cv2.imshow("Iceberg Measurement", img2 )
        cv2.setMouseCallback('Iceberg Measurement', points, param = clicked_points)

        if key & 0xFF == ord('1'):
            if rheight not in heights and rheight != 0:
                heights.append(rheight)
                print("New Height: ", rheight)

        return clicked_points

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
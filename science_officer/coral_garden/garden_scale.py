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

FISHEYE_INVALID = False
PINHOLE_INVALID = False

HORI_REF = 49

def parse_args():
    parser = argparse.ArgumentParser(description="Measure the coral garden scale with BlueStar")

    common_args.video_args(parser)
    common_args.fisheye_args(parser)
    common_args.measurement_args(parser)

    parser.add_argument(
        "--disable-dual-ref",
        default=False,
        action="store_true",
        help="Disable using the vertical pole for measurement refrence",
    )

    return parser.parse_args()

args = parse_args()

# Initialize image source
try:
    config = frame_capture.FrameSourceConfig.from_args(args)
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
        freeze = False

        if args.fisheye_type == "pinhole" and PINHOLE_INVALID == False:
            map1, map2 = opencv_helpers.prepare_pinhole(K_PINHOLE, D_PINHOLE, (resW, resH))
        elif args.fisheye_type == "fisheye" and FISHEYE_INVALID == False:
            balance = 0.3
            map1, map2 = opencv_helpers.prepare_fisheye(K_FISHEYE, D_FISHEYE, (resW, resH), balance)

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
            if key == ord('2'):
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

            if not freeze:
                clicked_points = draw_mode(img1,heights,clicked_points)
            else: 
                clicked_points = draw_mode(photo,heights,clicked_points)
            
        return heights
    
def draw_mode(picture,heights, clicked_points):

        imgconst = picture.copy()
        global rheight
        rheight = 0
        
        img2 = imgconst.copy()
        
        point_colours = [
            (0, 255, 0),
            (0, 255, 0),
            (0, 0, 255),
            (0, 0, 255),
            (255, 0, 0),
            (255, 0, 0),
        ]
        for i, point in enumerate(clicked_points):
            cv2.circle(img2, point, 10, point_colours[i], -1)

        if args.follow_mouse == True:
            overlay = img2.copy()

            if len(clicked_points) == 1: 
                cv2.line(overlay, clicked_points[0], (mouse_x, mouse_y), point_colours[0], 5,)
            elif len(clicked_points) == 2 and args.disable_dual_ref == False:
                cv2.line(overlay, clicked_points[1], (mouse_x, mouse_y), point_colours[1], 5,)
            elif len(clicked_points) == 3: 
                cv2.line(overlay, clicked_points[2], (mouse_x, mouse_y), point_colours[2], 5,)
            elif len(clicked_points) == 4 and args.disable_dual_ref == False:
                cv2.line(overlay, clicked_points[3], (mouse_x, mouse_y), point_colours[3], 5,)
                cv2.line(overlay, clicked_points[1], (mouse_x, mouse_y), point_colours[3], 5,)
            elif len(clicked_points) == 5: 
                cv2.line(overlay, clicked_points[4], (mouse_x, mouse_y), point_colours[4], 5,)            
            
            alpha = 0.4
            img2 = cv2.addWeighted(overlay, alpha, img2, 1 - alpha, 0)
        
        if args.disable_dual_ref == False:
            if len(clicked_points) >= 2: # Bottom left to mid left upright
                cv2.line(img2, clicked_points[0], clicked_points[1], (0, 255, 0), 5,)

            if len(clicked_points) >= 3: # mid left upright to top left
                cv2.line(img2, clicked_points[1], clicked_points[2], (0, 0, 255), 5,)

            if len(clicked_points) >= 4: # top left to top right
                cv2.line(img2, clicked_points[2], clicked_points[3], (255, 0, 0), 5,)

            if len(clicked_points) >= 5: # top right to mid right upright
                cv2.line(img2, clicked_points[3], clicked_points[4], (255, 0, 0), 5,)
                cv2.line(img2, clicked_points[1], clicked_points[4], (255, 0, 0), 5,) # mid left upright to mid right upright
            
            if len(clicked_points) >= 6: # mid right upright to bottom left
                cv2.line(img2, clicked_points[4], clicked_points[5], (255, 0, 0), 5,)
            
                known_length_top_px = line_distance(clicked_points[2], clicked_points[3])
                known_length_bottom_px = line_distance(clicked_points[1], clicked_points[4])

                coral_garden_length_px = line_distance(clicked_points[0], clicked_points[5])

                coral_garden_length_alt_px = line_distance(clicked_points[0], clicked_points[1]) + line_distance(clicked_points[1], clicked_points[4]) + line_distance(clicked_points[4], clicked_points[5])

                coral_garden_height_1_px = line_distance(clicked_points[1], clicked_points[2])
                coral_garden_height_2_px = line_distance(clicked_points[3], clicked_points[4])

                coral_garden_known_avg_px = sum((known_length_top_px, known_length_bottom_px)) / len((known_length_top_px, known_length_bottom_px))
                coral_garden_height_avg_px = sum((coral_garden_height_1_px, coral_garden_height_2_px)) / len((coral_garden_height_1_px, coral_garden_height_2_px))

                if coral_garden_height_avg_px != 0 and coral_garden_length_px != 0 and coral_garden_known_avg_px != 0:
                    horizontal_cm_per_px = HORI_REF / coral_garden_known_avg_px

                    coral_garden_height_cm = coral_garden_height_avg_px * horizontal_cm_per_px
                    coral_garden_length_cm = coral_garden_length_px * horizontal_cm_per_px
                    coral_garden_length_alt_cm = coral_garden_length_alt_px * horizontal_cm_per_px

                    real_height = round(coral_garden_height_cm, 2)
                    real_length = round(coral_garden_length_cm, 2)
                    real_length_alt = round(coral_garden_length_alt_cm, 2)

                    opencv_helpers.text_with_background(img2, f"Ref: {HORI_REF}cm", (10,30))
                    opencv_helpers.text_with_background(img2, f"Height: {real_height}cm", (10,70))
                    opencv_helpers.text_with_background(img2, f"Length: {real_length}cm", (10,110))
                    opencv_helpers.text_with_background(img2, f"Length Alt: {real_length_alt}cm", (10,150))

                else:
                    print("Invalid points:")
                    clicked_points = []
        else:
            if len(clicked_points) > 1:
                cv2.line(img2, clicked_points[0], clicked_points[1],(0,0,255),5)
            if len(clicked_points) > 3:
                cv2.line(img2, clicked_points[2], clicked_points[3],(0,255,0),5)
            if len(clicked_points) == 6:
                cv2.line(img2, clicked_points[4], clicked_points[5],(255,0,0),5)
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
                    opencv_helpers.text_with_background(img2, f"Ref: {ref_width}cm", (10,30))
                    opencv_helpers.text_with_background(img2, f"G: {rheight}cm", (10,70))
                    opencv_helpers.text_with_background(img2, f"B: {rwidth}cm", (10,110))
                    
                else:
                    print(f"Invalid Points!!! {ref_width} {width_pxdistance} {height_pxdistance}")
        
        if mouse_x != -1:
            opencv_helpers.draw_zoom_cursor(img2, img2, (mouse_x, mouse_y), zoom=3.0, lens_radius=150)
            
        cv2.imshow("Coral Garden Measurement", img2 )
        cv2.setMouseCallback('Coral Garden Measurement', points, param = clicked_points)

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
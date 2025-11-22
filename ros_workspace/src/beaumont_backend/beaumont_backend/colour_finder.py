# This code runs without ROS2 as it's just for testing
import cv2
import numpy as np
import sys
import tkinter as tk
from tkinter import filedialog
import easygui

image_hsv = None
pixel = (0,0,0) #RANDOM DEFAULT VALUE

ftypes = [
    ("JPG", "*.jpg;*.JPG;*.JPEG"), 
    ("PNG", "*.png;*.PNG"),
    ("GIF", "*.gif;*.GIF"),
    ("All files", "*.*")
]

def check_boundaries(value, tolerance, ranges, upper_or_lower):
    if ranges == 0:
        # set the boundary for hue
        boundary = 180
    elif ranges == 1:
        # set the boundary for saturation and value
        boundary = 255

    if upper_or_lower == 1:
            value = value + tolerance
            if value>boundary:
                value = boundary
    else:
        value = value - tolerance
        if value<0:
            value = 0

    return value

def pick_color(event,x,y,flags,param):
    if event == cv2.EVENT_LBUTTONDOWN:
        pixel = image_hsv[y,x]

        #HUE, SATURATION, AND VALUE (BRIGHTNESS) RANGES. TOLERANCE COULD BE ADJUSTED.
        # Set range = 0 for hue and range = 1 for saturation and brightness
        # set upper_or_lower = 1 for upper and upper_or_lower = 0 for lower
        hue_upper = check_boundaries(pixel[0], 20, 0, 1)
        hue_lower = check_boundaries(pixel[0], 20, 0, 0)
        saturation_upper = check_boundaries(pixel[1], 20, 1, 1)
        saturation_lower = check_boundaries(pixel[1], 20, 1, 0)
        value_upper = check_boundaries(pixel[2], 40, 1, 1)
        value_lower = check_boundaries(pixel[2], 40, 1, 0)

        lower =  np.array([hue_lower, saturation_lower, value_lower])
        upper =  np.array([hue_upper, saturation_upper, value_upper])
        print(lower, upper)

        #A MONOCHROME MASK FOR GETTING A BETTER VISION OVER THE COLORS 
        image_mask = cv2.inRange(image_hsv,lower,upper)
        
        cv2.imshow("Mask",image_mask)
        cv2.moveWindow("Mask", 300, 410+100)

def main():

    global image_hsv, pixel

    #OPEN DIALOG FOR READING THE IMAGE FILE
    root = tk.Tk()
    root.withdraw() #HIDE THE TKINTER GUI
    # file_path = filedialog.askopenfilename(filetypes = ftypes)
    file_path = easygui.fileopenbox(msg="Choose a file")
    root.update()
    image_src = cv2.imread(file_path)
    height, width, _ = image_src.shape
    aspect_ratio = width / height

    cv2.imshow("BGR",image_src)
    cv2.moveWindow("BGR", 0+300, 0+100)

    # bilateral_filtered = cv2.bilateralFilter(image_src, 25, 100, 100)

    # cv2.imshow("Bilateral",image_src)

    #CREATE THE HSV FROM THE BGR IMAGE
    image_hsv = cv2.cvtColor(image_src,cv2.COLOR_BGR2HSV)
    cv2.imshow("HSV",image_hsv)
    cv2.moveWindow("HSV", 514+300, 0+100)

    # CALLBACK FUNCTIONS
    cv2.setMouseCallback("BGR", pick_color)
    cv2.setMouseCallback("HSV", pick_color)

    cv2.waitKey(0)
    cv2.destroyAllWindows()

if __name__=='__main__':
    main()
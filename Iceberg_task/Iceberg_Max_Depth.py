import cv2
import numpy as np
import matplotlib.pyplot as plt
import os
from math import sqrt


def main():
    #open default camera
    cap = cv2.VideoCapture(1)

    #check if opened
    if not cap.isOpened():
        print("Error: Could not open video device.")
        return

    #Resolution
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)

    while True:
        # Capture frame-by-frame
        ret, frame = cap.read()

        #Nothing was returned
        if not ret:
            print("Error: Failed to capture frame.")
            break
        
        #Honestly forget what this does
        #i think it checks for the last two bytes to indicate letter "q" so press q
        #if in loop it would probably be useful for screenshotting if wanted
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
        
        #Renaming variable
        img = frame[:]
        #Convert to Grayscale
        gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)

        #Makes anything beyond the threshold black, with weird parameters:

        #ret, thresh = cv2.threshold(
        #    gray,0,255,
        #    cv2.THRESH_BINARY_INV+cv2.THRESH_OTSU
        #)

        #Gaussian attempt, uses blur before threshold to remove sharp edges
        blur = cv2.GaussianBlur(gray,(3,3),2)
        ret,thresh = cv2.threshold(blur,0,255,cv2.THRESH_BINARY+cv2.THRESH_OTSU)
        
        #find contours (Edges) of each object
        contours, hierarchy = cv2.findContours(
            thresh, cv2.RETR_EXTERNAL,
            cv2.CHAIN_APPROX_SIMPLE
            #cv2.CHAIN_APPROX_NONE (Alternative type, more precision but ugly)
        )
        
        #For all contours find their area
        for cnt in contours:
            area = cv2.contourArea(cnt)

            #draw bounding box
            x,y,w,h = cv2.boundingRect(cnt)
            #Specific constant used for measuring the height relative to length
            heightrate = 60/w
            #used constant to calculate height (Assumption that the width and height
            #are both visible at once)
            adjusted_h = round(h*heightrate,1)

            #Alternative with slanted rectangles, sucks to work with
            #rect = cv2.minAreaRect(cnt)
            #box=cv2.boxPoints(rect)
            #box = np.array(box,dtype=int)
            
            #filter out dots of 0 or 1 area
            if area > 100:

                #Draw the rectangle on the camera feed
                cv2.rectangle(img,(x,y), (x+w,y+h),(0,255,0),2)

                #cv2.drawContours(img,[box], 0,(0,255,0),2)

                #Place text for width and height
                cv2.putText(img,f"W{w}px, H{adjusted_h}cm",(x,y),cv2.FONT_HERSHEY_SIMPLEX,
                            1,(0,0,255),2)
                
                #cv2.putText(img,str(area),(x,y),cv2.FONT_HERSHEY_SIMPLEX,
                            #1,(0,0,255),2)
                #cv2.putText(img,str(area),box[0],cv2.FONT_HERSHEY_SIMPLEX,
                         #   1,(0,0,255),2)
        #Show modified image
        cv2.imshow("image",img)

    clicked_points = []
    def points(event,x,y,flags,param):
        clicked_points = param
        if event == cv2.EVENT_LBUTTONDOWN:
            if len(clicked_points) == 4:
                clicked_points = []
            cv2.circle(img2,(x,y),2,(255,255,0),-1)
            clicked_points.append((x,y))
            print(f"Point added: ({x}, {y})")
    def line_distance(p1,p2):
        x_dif = p2[0]-p1[0]
        y_dif = p2[1]-p1[1]
        distance = sqrt(x_dif**2 + y_dif**2)
        #print(distance,p1,p2)
        return distance
    
    cv2.setMouseCallback('image', points,param = clicked_points)
    ret, frame = cap.read()
    #Nothing was returned
    if not ret:
            print("Error: Failed to capture frame.")
            
    while True:
        img2 = frame[:]

        cv2.imshow("image", img2 )
        if len(clicked_points) > 1:
            cv2.line(img2, clicked_points[0], clicked_points[1],(255,0,0),3)
        if len(clicked_points) == 4:
            cv2.line(img2, clicked_points[2], clicked_points[3],(0,0,255),3)
            width_pxdistance = line_distance(clicked_points[0],clicked_points[1])
            height_pxdistance = line_distance(clicked_points[2],clicked_points[3])
            
            rwidth = 60
            rheight = rwidth/width_pxdistance*height_pxdistance
            rheight = round(rheight,2)
        
            cv2.putText(img2,f"{rwidth}cm",clicked_points[0],cv2.FONT_HERSHEY_SIMPLEX,
                                1,(0,0,255),2)
            cv2.putText(img2,f"{rheight}cm",clicked_points[3],cv2.FONT_HERSHEY_SIMPLEX,
                                1,(0,0,255),2)
            cv2.imshow("image", img2 )
        

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    #End video feed
    cap.release()
    #destroy window
    cv2.destroyAllWindows()

main()

import cv2
from math import sqrt

def points(event,x,y,flags,param):
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


def cam_mode(cap):

        #check if opened
        if not cap.isOpened():
            print("Error: Could not open video device.")
            return

        #Resolution
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

        heights = []
        clicked_points = []
        auto = True
        while True:
            # Capture frame-by-frame
            ret, frame = cap.read()

            #Nothing was returned
            if not ret:
                print("Error: Failed to capture frame.")
                break
            
            #Renaming variable
            global img1
            img1 = frame[:]
            cv2.imshow("image", img1 )
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
            
            if key & 0xFF == ord('3'):
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
        if len(clicked_points) == 4:
            cv2.line(img2, clicked_points[2], clicked_points[3],(0,0,255),5)
            width_pxdistance = line_distance(clicked_points[0],clicked_points[1])
            height_pxdistance = line_distance(clicked_points[2],clicked_points[3])
            
            rwidth = 60
            if rwidth != 0 and width_pxdistance != 0 and height_pxdistance != 0:
                rheight = rwidth/width_pxdistance*height_pxdistance
                rheight = round(rheight,2)
        
                cv2.putText(img2,f"{rwidth}cm",clicked_points[0],cv2.FONT_HERSHEY_SIMPLEX,
                                    1,(0,0,255),2)
                cv2.putText(img2,f"{rheight}cm",clicked_points[3],cv2.FONT_HERSHEY_SIMPLEX,
                                    1,(0,0,255),2)
            else:
                 print(f"Invalid Points!!! {rwidth} {width_pxdistance} {height_pxdistance}")
            
        cv2.imshow("snap", img2 )
        cv2.setMouseCallback('snap', points, param = clicked_points)

        if key & 0xFF == ord('1'):
            if rheight not in heights and rheight != 0:
                heights.append(rheight)
                print("New Height: ", rheight)


                

def main():
    #open default camera
    cap = cv2.VideoCapture("rtsp://192.168.137.200:8554/cam2")
    global mouse_x,mouse_y
    mouse_x,mouse_y = -1,-1

    global mode
    mode = 1
    clicked_points = []
    
    while True:

        if mode == 3:
            #quit
            cap.release()
            cv2.destroyAllWindows()
            N = len(heights)
            avg = sum(heights)/N
            avg = round(avg,2)
            print(f"Average Depth {avg}")
            break
        elif mode == 1:
            heights = cam_mode(cap)
            print(heights)

main()

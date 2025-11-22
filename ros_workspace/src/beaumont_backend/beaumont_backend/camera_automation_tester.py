##### this code runs without ROS (it is just testing)
import cv2
import numpy as np
from PIL import Image
cam = cv2.VideoCapture(0)
 
Red = [255, 0, 0] ## red in RGB colorspace


while True: ### this opens the camera
    check, frame = cam.read()

    hsvImage = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    
    lower_range_red = np.array([160, 100, 100],dtype=np.uint8)
    upper_range_red = np.array([175, 255, 255],dtype=np.uint8) ### just need to change this a bit 
    

    the_object = cv2.inRange(hsvImage,lower_range_red ,upper_range_red )

    convert_to_pillow = Image.fromarray(the_object)

    box = convert_to_pillow.getbbox()

    
    
    if box is not None: ### this line make the bot identify the object with a bonding box 
        x1, y1, x2, y2 = box

        frame = cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 5) ## this line rap the object with a bounding box of color yellow with depth 5

    cv2.imshow("frame", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break
cam.release()
cv2.destroyAllWindows()




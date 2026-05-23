# Science Officer - Coral Garden
## webodm_upload.py
Used by the [BlueStar Science Officer GUI](/ros_workspace/src/bluestar_officer_frontend/) for uploading to WebODM automatically.


## garden_scale.py
Get the actual scale of the coral garden using a known measurement.

### Keys:
| Key | Action                |
|-----|-----------------------|
| Q   | Quit                  |
| 2   | Capture Frame / Pause |
| 3   | Reset Points          |

### Examples:
RTSP w/ ffmpeg backend
```
python3 garden_scale.py --source-type video --capture-backend ffmpeg --resolution 1280x720 --source rtsp://192.168.137.200:8554/cam2
```
USB w/ OpenCV backend
```
python3 garden_scale.py --source-type usb --capture-backend opencv --resolution 1280x720 --source usb0
```
RTSP w/ ffmpeg backend and fisheye correction
```
python3 garden_scale.py --source-type video --capture-backend ffmpeg --resolution 1280x720 --source rtsp://192.168.137.200:8554/cam2
```
USB w/ OpenCV backend and fisheye correction
```
python3 garden_scale.py --source-type usb --capture-backend opencv --resolution 1280x720 --source usb0
```
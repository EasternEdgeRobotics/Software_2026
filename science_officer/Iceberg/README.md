# Iceburg Measurement
## Keys:
| Key | Action                    |
|-----|---------------------------|
| Q   | Quit                      |
| 2   | Capture Frame / Pause     |
| 3   | Reset Points              |
| 4   | Toggle Fisheye Correction |

## Examples:
RTSP w/ ffmpeg backend
```
python3 Iceberg_Max_Depth.py --source-type video --capture-backend ffmpeg --resolution 1280x720 --source rtsp://192.168.137.200:8554/cam
```
USB w/ OpenCV backend
```
python3 Iceberg_Max_Depth.py --source-type usb --capture-backend opencv --resolution 1280x720 --source usb0
```
RTSP w/ ffmpeg backend and fisheye correction
```
python3 Iceberg_Max_Depth.py --source-type video --capture-backend ffmpeg --resolution 1280x720 --source rtsp://192.168.137.200:8554/cam --fisheye-correction
```
USB w/ OpenCV backend and fisheye correction
```
python3 Iceberg_Max_Depth.py --source-type usb --capture-backend opencv --resolution 1280x720 --source usb0 --fisheye-correction
```
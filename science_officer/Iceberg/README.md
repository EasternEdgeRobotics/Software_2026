# Iceburg Measurement
Examples:
RTSP w/ ffmpeg backend
```
python3 Iceberg_Max_Depth.py --source-type video --capture-backend ffmpeg --resolution 1280x720 --source rtsp://192.168.137.200:8554/cam2
```
USB w/ OpenCV backend
```
python3 Iceberg_Max_Depth.py --source-type usb --capture-backend opencv --resolution 1280x720 --source usb0
```
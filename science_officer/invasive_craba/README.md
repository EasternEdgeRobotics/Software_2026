# Invasive Crab Detection
Examples:
```
python3 yolo_detect.py --model-path Standard_Model.pt --source-type vidoe --source rtsp://192.168.137.200:8889/cam --min-thresh 0.65 --resolution 1280x720
```

```
python3 yolo_detect.py --model-path Standard_Model.pt --source-type usb --source usb0 --min-thresh 0.65 --resolution 1280x720
```
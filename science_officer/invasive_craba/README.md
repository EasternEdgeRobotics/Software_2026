# Invasive Crab Detection
## Required Packages:
Generic: `pip install opencv-python ultralytics numpy`

macOS (Python 3.13 and lower): `pip install opencv-python ultralytics numpy coremltools`

## Keys:
| Key | Action             |
|-----|--------------------|
| Q   | Quit               |
| 1   | Pause Inference    |
| 2   | Take Photo         |
| -/_ | Lower Threshold    |
| +/= | Increase Threshold |


## Examples:
### RTSP w/ CPU Inference & ffmpeg backend
```
python3 yolo_detect.py --model-path Standard_Model/Standard_Model.pt --source-type video --source rtsp://192.168.137.200:8889/cam --min-thresh 0.65 --resolution 1280x720 --device cpu --capture-backend ffmpeg
```

### USB w/ CPU Inference
```
python3 yolo_detect.py --model-path Standard_Model/Standard_Model.pt --source-type usb --source usb0 --min-thresh 0.65 --resolution 1280x720 --device cpu --capture-backend opencv
```

### RTSP w/ MPS Inference (macOS Only)
```
python3 yolo_detect.py --model-path Standard_Model/Standard_Model.pt --source-type video --source rtsp://192.168.137.200:8554/cam --min-thresh 0.65 --resolution 1280x720 --device mps --capture-backend ffmpeg
```

### USB w/ MPS Inference (macOS Only)
```
python3 yolo_detect.py --model-path Standard_Model/Standard_Model.pt --source-type usb --source usb0 --min-thresh 0.65 --resolution 1280x720 --device mps --capture-backend opencv
```

### RTSP w/ CoreML Inference (macOS Only)
```
python3 yolo_detect.py --model-path Standard_Model/Standard_Model.mlpackage --source-type video --source rtsp://192.168.137.200:8554/cam --min-thresh 0.65 --resolution 1280x720 --capture-backend ffmpeg
```

### USB w/ CoreML Inference (macOS Only)
```
python3 yolo_detect.py --model-path Standard_Model/Standard_Model.mlpackage --source-type usb --source usb0 --min-thresh 0.65 --resolution 1280x720 --capture-backend opencv
```

## Updating the CoreML version of the model
When `Standard_Model.pt` is updated run the following to update the CoreML version of the model:
```
yolo export model=Standard_Model.pt format=coreml imgsz=640 half=True nms=True
```
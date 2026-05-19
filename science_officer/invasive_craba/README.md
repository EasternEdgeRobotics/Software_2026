# Invasive Crab Detection
## Required Packages:
Generic: `pip install opencv-python ultralytics numpy`
macOS (Python 3.13 and lower): `pip install opencv-python ultralytics numpy coremltools`

## Examples:
### RTSP w/ CPU Inference
```
python3 yolo_detect.py --model-path Standard_Model.pt --source-type vidoe --source rtsp://192.168.137.200:8889/cam --min-thresh 0.65 --resolution 1280x720 --device cpu
```

### USB w/ CPU Inference
```
python3 yolo_detect.py --model-path Standard_Model.pt --source-type usb --source usb0 --min-thresh 0.65 --resolution 1280x720 --device cpu
```

### RTSP w/ MPS Inference (macOS Only)
```
python3 yolo_detect.py --model-path Standard_Model.pt --source-type vidoe --source rtsp://192.168.137.200:8889/cam --min-thresh 0.65 --resolution 1280x720 --device mps
```

### USB w/ MPS Inference (macOS Only)
```
python3 yolo_detect.py --model-path Standard_Model.pt --source-type usb --source usb0 --min-thresh 0.65 --resolution 1280x720 --device mps
```

### RTSP w/ CoreML Inference (macOS Only)
```
python3 yolo_detect.py --model-path Standard_Model.mlpackage --source-type vidoe --source rtsp://192.168.137.200:8889/cam --min-thresh 0.65 --resolution 1280x720
```

### USB w/ CoreML Inference (macOS Only)
```
python3 yolo_detect.py --model-path Standard_Model.mlpackage --source-type usb --source usb0 --min-thresh 0.65 --resolution 1280x720
```

## Updating the CoreML version of the model
When `Standard_Model.pt` is updated run the following to update the CoreML version of the model:
```
yolo export model=Standard_Model.pt format=coreml imgsz=640 half=True nms=True
```
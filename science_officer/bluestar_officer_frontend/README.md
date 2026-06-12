# BlueStar Science Officer GUI
The BlueStar Science Officer GUI is a minimal version of the main BlueStar GUI. removing all dependancy on ROS to allow for macOS support.

## Dependancies:
macOS (brew):
```
brew install gstreamer libnice libnice-gstreamer libtiff
```

## Building
To build on a macOS systems run the following:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

At compile time, you can choose the camera backend used in the officer frontend. By default it will use a backend based off gstreamer's webrtcbin, but a backend based off gstrreamer's whepclientsrc can be selected by building with the following argument:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBLUESTAR_CAMERA_BACKEND=whepclientsrc
```
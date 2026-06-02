# BlueStar Science Officer GUI
The BlueStar Science Officer GUI is a minimal version of the main BlueStar GUI. removing all dependancy on ROS to allow for macOS support.

## Building
To build on a macOS systems run the following:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
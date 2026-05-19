#pragma once

#include "streaming/CameraStream.hpp"

#include <memory>

std::unique_ptr<CameraStream> createCameraStream();

const char* cameraStreamBackendName();
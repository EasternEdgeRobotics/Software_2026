#include "streaming/CameraStreamFactory.hpp"

#if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
#include "streaming/backend_webrtcbin.hpp"
#elif defined(BLUESTAR_CAMERA_BACKEND_WHEPCLIENTSRC)
#include "streaming/backend_whepclientsrc.hpp"
#else
#error "No camera backend selected"
#endif

std::unique_ptr<CameraStream> createCameraStream() {
#if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
    return std::make_unique<WebRtcBinStream>();
#elif defined(BLUESTAR_CAMERA_BACKEND_WHEPCLIENTSRC)
    return std::make_unique<WhepClientStream>();
#endif
}

const char* cameraStreamBackendName() {
#if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
    return "GStreamer webrtcbin";
#elif defined(BLUESTAR_CAMERA_BACKEND_WHEPCLIENTSRC)
    return "GStreamer whepclientsrc";
#endif
}
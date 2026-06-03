#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FrameData {
    std::vector<uint8_t> y_plane;
    std::vector<uint8_t> uv_plane;
    int width = 0;
    int height = 0;
    bool dirty = false;
};

struct StreamConfig {
    std::string url;

    // Optional backend-specific caps. Backends may ignore these.
    std::string video_caps;
    std::string audio_caps;

    std::string label;
};

class CameraStream {
public:
    virtual ~CameraStream() = default;

    virtual bool start(const StreamConfig& cfg) = 0;
    virtual void stop() = 0;
    virtual bool poll_frame(FrameData& out) = 0;
    virtual bool failed() const = 0;
};
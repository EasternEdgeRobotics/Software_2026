#pragma once

#include "streaming/CameraStream.hpp"

#include <gst/gst.h>

#include <atomic>
#include <mutex>

class WebRtcBinStream : public CameraStream {
public:
    WebRtcBinStream();
    ~WebRtcBinStream() override;

    bool start(const StreamConfig& cfg) override;
    void stop() override;
    bool poll_frame(FrameData& out) override;
    bool failed() const override;

private:
    std::atomic<bool> stopping_{false};
    std::atomic<bool> failed_{false};

    StreamConfig cfg_;

    std::mutex frame_mutex_;
    FrameData pending_frame_;
};
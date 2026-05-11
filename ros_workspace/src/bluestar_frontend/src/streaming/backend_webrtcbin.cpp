#include "streaming/backend_webrtcbin.hpp"

#include <iostream>

WebRtcBinStream::WebRtcBinStream() = default;

WebRtcBinStream::~WebRtcBinStream() {
    stop();
}

bool WebRtcBinStream::start(const StreamConfig& cfg) {
    stopping_ = false;
    failed_ = false;
    cfg_ = cfg;

    std::cerr << "[" << cfg_.label
              << "] WebRtcBinStream backend selected, but implementation is "
              << "not wired in yet\n";

    failed_ = true;
    return false;
}

void WebRtcBinStream::stop() {
    stopping_ = true;
}

bool WebRtcBinStream::poll_frame(FrameData& out) {
    std::lock_guard<std::mutex> lock(frame_mutex_);

    if (!pending_frame_.dirty) {
        return false;
    }

    out = pending_frame_;
    pending_frame_.dirty = false;
    return true;
}

bool WebRtcBinStream::failed() const {
    return failed_.load();
}
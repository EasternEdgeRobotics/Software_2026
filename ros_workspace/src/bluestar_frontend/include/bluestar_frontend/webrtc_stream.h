#pragma once
#include <gst/gst.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>

struct FrameData {
    std::vector<uint8_t> y_plane;
    std::vector<uint8_t> uv_plane;
    int  width  = 0;
    int  height = 0;
    bool dirty  = false;
};

struct StreamConfig {
    std::string whep_url;
    // Optional: RTP/codec caps forwarded to whepclientsrc's video-caps /
    // audio-caps properties.  Leave empty to use the element's defaults
    // (H264, VP8, VP9, H265, AV1 / OPUS).
    std::string video_caps;
    std::string audio_caps;
    std::string label;
};

class WebRTCStream {
public:
    WebRTCStream();
    ~WebRTCStream();
    
    bool start(const StreamConfig& cfg);
    void stop();
    bool poll_frame(FrameData& out);

    void on_pad_added(GstPad* pad);

private:
    GstElement* pipeline_      = nullptr;
    GstElement* whepclientsrc_ = nullptr;
    GstElement* appsink_       = nullptr;
    std::atomic<bool> stopping_{false};
    bool video_linked_ = false;
    bool audio_linked_ = false;

    StreamConfig cfg_;
    std::mutex   frame_mutex_;
    FrameData    pending_frame_;

    static GstFlowReturn on_new_sample(GstElement* sink, gpointer data);
    static GstBusSyncReply on_bus_sync(
        GstBus* bus,
        GstMessage* msg,
        gpointer data);
};
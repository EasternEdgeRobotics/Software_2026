#pragma once

#include "streaming/CameraStream.hpp"

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>

#include <atomic>
#include <mutex>
#include <string>

class WebRtcBinStream : public CameraStream {
public:
    WebRtcBinStream();
    ~WebRtcBinStream() override;

    bool start(const StreamConfig& cfg) override;
    void stop() override;
    bool poll_frame(FrameData& out) override;
    bool failed() const override;

private:
    GstElement* pipeline_ = nullptr;
    GstElement* webrtcbin_ = nullptr;
    GstElement* appsink_ = nullptr;

    std::atomic<bool> stopping_{false};
    std::atomic<bool> failed_{false};

    bool video_linked_ = false;
    bool audio_linked_ = false;

    StreamConfig cfg_;

    std::mutex frame_mutex_;
    FrameData pending_frame_;

    void on_offer_created(GstPromise* promise);
    void on_ice_candidate(guint mline_index, gchararray candidate);
    void on_pad_added(GstPad* pad);

    bool do_whep_exchange(
        const std::string& offer_sdp,
        std::string& out_answer_sdp);

    static GstFlowReturn on_new_sample(GstElement* sink, gpointer data);

    static GstBusSyncReply on_bus_sync(
        GstBus* bus,
        GstMessage* msg,
        gpointer data);
};
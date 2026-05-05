#include "webrtc_stream.h"

#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <cstring>
#include <iostream>

WebRTCStream::WebRTCStream()  = default;
WebRTCStream::~WebRTCStream() { stop(); }

// ---------------------------------------------------------------------------
// start — create pipeline + whepclientsrc; all WHEP signalling, SDP
// negotiation, ICE, RTP depayloading, and decoding happen inside the element.
// ---------------------------------------------------------------------------
bool WebRTCStream::start(
    const StreamConfig& cfg,
    GMainContext* shared_context) {
    cfg_ = cfg;

    g_main_context_push_thread_default(shared_context);

    pipeline_ = gst_pipeline_new(nullptr);
    whepclientsrc_ = gst_element_factory_make("whepclientsrc", nullptr);

    if (!pipeline_ || !whepclientsrc_) {
        std::cerr << "[" << cfg_.label << "] Failed to create pipeline or "
                     "whepclientsrc\n";
        g_main_context_pop_thread_default(shared_context);
        return false;
    }

    gst_child_proxy_set(
        GST_CHILD_PROXY(whepclientsrc_),
        "signaller::whep-endpoint",
        cfg_.whep_url.c_str(),
        nullptr);

    g_object_set(whepclientsrc_, "stun-server", NULL, nullptr);

    if (!cfg_.video_caps.empty()) {
        GstCaps* vcaps = gst_caps_from_string(cfg_.video_caps.c_str());
        if (vcaps) {
            g_object_set(whepclientsrc_, "video-caps", vcaps, nullptr);
            gst_caps_unref(vcaps);
        }
    }

    if (!cfg_.audio_caps.empty()) {
        GstCaps* acaps = gst_caps_from_string(cfg_.audio_caps.c_str());
        if (acaps) {
            g_object_set(whepclientsrc_, "audio-caps", acaps, nullptr);
            gst_caps_unref(acaps);
        }
    }

    gst_bin_add(GST_BIN(pipeline_), whepclientsrc_);

    g_signal_connect(
        whepclientsrc_,
        "pad-added",
        G_CALLBACK(+[](GstElement*, GstPad* pad, gpointer d) {
            static_cast<WebRTCStream*>(d)->on_pad_added(pad);
        }),
        this);

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    gst_bus_set_sync_handler(bus, on_bus_sync, this, nullptr);
    gst_object_unref(bus);

    gst_element_set_state(pipeline_, GST_STATE_PLAYING);

    g_main_context_pop_thread_default(shared_context);
    return true;
}

GstBusSyncReply WebRTCStream::on_bus_sync(
    GstBus*,
    GstMessage* msg,
    gpointer data) {
    auto* self = static_cast<WebRTCStream*>(data);
    if (!self) {
        return GST_BUS_PASS;
    }

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* debug_info = nullptr;
            gst_message_parse_error(msg, &err, &debug_info);

            std::cerr << "[" << self->cfg_.label << "] GStreamer error: "
                      << (err ? err->message : "unknown") << "\n";

            if (debug_info) {
                std::cerr << "  Debug: " << debug_info << "\n";
            }

            g_clear_error(&err);
            g_free(debug_info);
            break;
        }

        case GST_MESSAGE_EOS:
            std::cerr << "[" << self->cfg_.label << "] End of stream\n";
            break;

        default:
            break;
    }

    return GST_BUS_PASS;
}

void WebRTCStream::stop() {
    if (pipeline_) {
        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
        gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
        gst_object_unref(bus);

        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }

    appsink_ = nullptr;
}

bool WebRTCStream::poll_frame(FrameData& out) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (!pending_frame_.dirty) return false;
    out = pending_frame_;
    pending_frame_.dirty = false;
    return true;
}

// ---------------------------------------------------------------------------
// on_pad_added — whepclientsrc exposes *decoded* pads:
//   video_%s_%u  →  video/x-raw (possibly with HW memory feature)
//   audio_%s_%u  →  audio/x-raw
//
// For video we colour-convert to NV12 (handles HW surface download too) and
// push into appsink.  For audio we route to autoaudiosink.
// ---------------------------------------------------------------------------
void WebRTCStream::on_pad_added(GstPad* pad) {
    if (stopping_) return;

    const std::string name = GST_PAD_NAME(pad);

    if (name.rfind("video_", 0) == 0) {
        if (video_linked_) return;
        video_linked_ = true;

        GstElement* convert = gst_element_factory_make("videoconvert", nullptr);
        GstElement* capsfilter =
            gst_element_factory_make("capsfilter", nullptr);
        GstElement* queue = gst_element_factory_make("queue", nullptr);
        GstElement* appsink = gst_element_factory_make("appsink", nullptr);

        if (!convert || !capsfilter || !queue || !appsink) {
            std::cerr << "[" << cfg_.label
                      << "] Failed to create video sink chain\n";
            return;
        }

        g_object_set(
            queue, "max-size-buffers", (guint)2, "leaky", (guint)2, nullptr);

        GstCaps* nv12 = gst_caps_from_string("video/x-raw,format=NV12");
        g_object_set(capsfilter, "caps", nv12, nullptr);
        gst_caps_unref(nv12);

        g_object_set(
            appsink,
            "emit-signals",
            TRUE,
            "sync",
            FALSE,
            "max-buffers",
            (guint)1,
            "drop",
            TRUE,
            nullptr);

        appsink_ = appsink;
        g_signal_connect(
            appsink, "new-sample", G_CALLBACK(on_new_sample), this);

        gst_bin_add_many(
            GST_BIN(pipeline_), convert, capsfilter, queue, appsink, nullptr);

        if (!gst_element_link_many(
                convert, capsfilter, queue, appsink, nullptr)) {
            std::cerr << "[" << cfg_.label
                      << "] Failed to link video chain\n";
            return;
        }

        gst_element_sync_state_with_parent(convert);
        gst_element_sync_state_with_parent(capsfilter);
        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(appsink);

        GstPad* sink = gst_element_get_static_pad(convert, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, sink);
        gst_object_unref(sink);

        if (ret != GST_PAD_LINK_OK) {
            std::cerr << "[" << cfg_.label
                      << "] Failed to link source pad to video chain\n";
        }

    } else if (name.rfind("audio_", 0) == 0) {
        return;
    }
}

// ---------------------------------------------------------------------------
// on_new_sample — unchanged; frame arrives as NV12 from capsfilter above.
// ---------------------------------------------------------------------------
GstFlowReturn WebRTCStream::on_new_sample(GstElement* sink, gpointer data) {
    auto* self = static_cast<WebRTCStream*>(data);
    if (!self || self->stopping_) return GST_FLOW_EOS;

    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) return GST_FLOW_ERROR;

    GstCaps* caps = gst_sample_get_caps(sample);
    GstBuffer* buf = gst_sample_get_buffer(sample);

    if (!caps || !buf) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstVideoInfo vinfo;
    if (!gst_video_info_from_caps(&vinfo, caps)) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstVideoFrame vframe;
    if (gst_video_frame_map(&vframe, &vinfo, buf, GST_MAP_READ)) {
        if (GST_VIDEO_FRAME_N_PLANES(&vframe) < 2) {
            gst_video_frame_unmap(&vframe);
            gst_sample_unref(sample);
            return GST_FLOW_OK;
        }

        const int w = GST_VIDEO_FRAME_WIDTH(&vframe);
        const int h = GST_VIDEO_FRAME_HEIGHT(&vframe);

        if (w <= 0 || h <= 0 || (w % 2) != 0 || (h % 2) != 0) {
            gst_video_frame_unmap(&vframe);
            gst_sample_unref(sample);
            return GST_FLOW_OK;
        }

        const int y_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0);
        const int uv_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 1);

        const auto* y_src = static_cast<const uint8_t*>(
            GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0));
        const auto* uv_src = static_cast<const uint8_t*>(
            GST_VIDEO_FRAME_PLANE_DATA(&vframe, 1));

        if (!y_src || !uv_src) {
            gst_video_frame_unmap(&vframe);
            gst_sample_unref(sample);
            return GST_FLOW_OK;
        }

        std::lock_guard<std::mutex> lock(self->frame_mutex_);
        auto& f = self->pending_frame_;
        f.width = w;
        f.height = h;

        f.y_plane.resize(static_cast<size_t>(w) * h);
        for (int row = 0; row < h; ++row) {
            memcpy(f.y_plane.data() + row * w, y_src + row * y_stride, w);
        }

        const int uv_rows = h / 2;
        f.uv_plane.resize(static_cast<size_t>(w) * uv_rows);
        for (int row = 0; row < uv_rows; ++row) {
            memcpy(f.uv_plane.data() + row * w, uv_src + row * uv_stride, w);
        }

        f.dirty = true;
        gst_video_frame_unmap(&vframe);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}
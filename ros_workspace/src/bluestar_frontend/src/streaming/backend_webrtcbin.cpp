#include "streaming/backend_webrtcbin.hpp"

#include <gst/app/gstappsink.h>
#include <gst/sdp/sdp.h>
#include <gst/video/video.h>
#include <curl/curl.h>

#include <cstring>
#include <iostream>
#include <string>

static constexpr const char* kDefaultVideoCaps =
    "application/x-rtp,media=video,encoding-name=H264,"
    "payload=127,clock-rate=90000";

static constexpr const char* kDefaultAudioCaps =
    "application/x-rtp,media=audio,encoding-name=PCMU,"
    "payload=0,clock-rate=8000";

static size_t curl_write_cb(
    char* ptr,
    size_t size,
    size_t nmemb,
    void* userdata) {
    auto* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

WebRtcBinStream::WebRtcBinStream() = default;

WebRtcBinStream::~WebRtcBinStream() {
    stop();
}

bool WebRtcBinStream::start(const StreamConfig& cfg) {
    stopping_ = false;
    failed_ = false;
    cfg_ = cfg;

    video_linked_ = false;
    audio_linked_ = false;

    pipeline_ = gst_pipeline_new(nullptr);
    webrtcbin_ = gst_element_factory_make("webrtcbin", nullptr);

    if (!pipeline_ || !webrtcbin_) {
        std::cerr << "[" << cfg_.label
                  << "] Failed to create pipeline or webrtcbin\n";
        failed_ = true;
        stop();
        return false;
    }

    g_object_set(
        webrtcbin_,
        "bundle-policy",
        GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE,
        "stun-server",
        NULL,
        nullptr);

    gst_bin_add(GST_BIN(pipeline_), webrtcbin_);

    g_signal_connect(
        webrtcbin_,
        "on-ice-candidate",
        G_CALLBACK(+[](GstElement*, guint idx, gchararray cand, gpointer d) {
            static_cast<WebRtcBinStream*>(d)->on_ice_candidate(idx, cand);
        }),
        this);

    g_signal_connect(
        webrtcbin_,
        "pad-added",
        G_CALLBACK(+[](GstElement*, GstPad* pad, gpointer d) {
            static_cast<WebRtcBinStream*>(d)->on_pad_added(pad);
        }),
        this);

    // Video transceiver.
    {
        const std::string& caps_string =
            cfg_.video_caps.empty() ? kDefaultVideoCaps : cfg_.video_caps;

        GstCaps* caps = gst_caps_from_string(caps_string.c_str());
        if (!caps) {
            std::cerr << "[" << cfg_.label
                      << "] Invalid video caps: " << caps_string << "\n";
            failed_ = true;
            stop();
            return false;
        }

        GstWebRTCRTPTransceiver* transceiver = nullptr;
        g_signal_emit_by_name(
            webrtcbin_,
            "add-transceiver",
            GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY,
            caps,
            &transceiver);

        gst_caps_unref(caps);

        if (transceiver) {
            gst_object_unref(transceiver);
        }
    }

    // Audio transceiver.
    {
        const std::string& caps_string =
            cfg_.audio_caps.empty() ? kDefaultAudioCaps : cfg_.audio_caps;

        GstCaps* caps = gst_caps_from_string(caps_string.c_str());
        if (!caps) {
            std::cerr << "[" << cfg_.label
                      << "] Invalid audio caps: " << caps_string << "\n";
            failed_ = true;
            stop();
            return false;
        }

        GstWebRTCRTPTransceiver* transceiver = nullptr;
        g_signal_emit_by_name(
            webrtcbin_,
            "add-transceiver",
            GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY,
            caps,
            &transceiver);

        gst_caps_unref(caps);

        if (transceiver) {
            gst_object_unref(transceiver);
        }
    }

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    gst_bus_set_sync_handler(bus, on_bus_sync, this, nullptr);
    gst_object_unref(bus);

    GstStateChangeReturn state_ret =
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);

    if (state_ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[" << cfg_.label
                  << "] Failed to set webrtcbin pipeline to PLAYING\n";
        failed_ = true;
        stop();
        return false;
    }

    GstPromise* promise = gst_promise_new_with_change_func(
        +[](GstPromise* p, gpointer d) {
            static_cast<WebRtcBinStream*>(d)->on_offer_created(p);
        },
        this,
        nullptr);

    g_signal_emit_by_name(webrtcbin_, "create-offer", nullptr, promise);

    return true;
}

void WebRtcBinStream::stop() {
    stopping_ = true;

    if (pipeline_) {
        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
        gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
        gst_object_unref(bus);

        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);

        pipeline_ = nullptr;
        webrtcbin_ = nullptr;
        appsink_ = nullptr;
    }

    video_linked_ = false;
    audio_linked_ = false;
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

GstBusSyncReply WebRtcBinStream::on_bus_sync(
    GstBus*,
    GstMessage* msg,
    gpointer data) {
    auto* self = static_cast<WebRtcBinStream*>(data);

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

            self->failed_ = true;

            g_clear_error(&err);
            g_free(debug_info);
            break;
        }

        case GST_MESSAGE_EOS:
            std::cerr << "[" << self->cfg_.label << "] End of stream\n";
            self->failed_ = true;
            break;

        default:
            break;
    }

    return GST_BUS_PASS;
}

void WebRtcBinStream::on_offer_created(GstPromise* promise) {
    if (stopping_ || !webrtcbin_) {
        gst_promise_unref(promise);
        return;
    }

    const GstStructure* reply = gst_promise_get_reply(promise);
    if (!reply) {
        std::cerr << "[" << cfg_.label
                  << "] Failed to create WebRTC offer: empty promise reply\n";
        failed_ = true;
        gst_promise_unref(promise);
        return;
    }

    GstWebRTCSessionDescription* offer = nullptr;
    gst_structure_get(
        reply,
        "offer",
        GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
        &offer,
        nullptr);

    gst_promise_unref(promise);

    if (!offer) {
        std::cerr << "[" << cfg_.label
                  << "] Failed to create WebRTC offer\n";
        failed_ = true;
        return;
    }

    GstPromise* local_promise = gst_promise_new();
    g_signal_emit_by_name(
        webrtcbin_,
        "set-local-description",
        offer,
        local_promise);
    gst_promise_interrupt(local_promise);
    gst_promise_unref(local_promise);

    gchar* offer_str = gst_sdp_message_as_text(offer->sdp);
    gst_webrtc_session_description_free(offer);

    if (!offer_str) {
        std::cerr << "[" << cfg_.label
                  << "] Failed to serialize WebRTC offer SDP\n";
        failed_ = true;
        return;
    }

    std::string answer_sdp;
    if (!do_whep_exchange(offer_str, answer_sdp)) {
        std::cerr << "[" << cfg_.label << "] WHEP exchange failed\n";
        failed_ = true;
        g_free(offer_str);
        return;
    }

    g_free(offer_str);

    GstSDPMessage* sdp_msg = nullptr;
    if (gst_sdp_message_new(&sdp_msg) != GST_SDP_OK || !sdp_msg) {
        std::cerr << "[" << cfg_.label
                  << "] Failed to allocate SDP message\n";
        failed_ = true;
        return;
    }

    GstSDPResult parse_ret = gst_sdp_message_parse_buffer(
        reinterpret_cast<const guint8*>(answer_sdp.data()),
        static_cast<guint>(answer_sdp.size()),
        sdp_msg);

    if (parse_ret != GST_SDP_OK) {
        std::cerr << "[" << cfg_.label
                  << "] Failed to parse WHEP answer SDP\n";
        failed_ = true;
        gst_sdp_message_free(sdp_msg);
        return;
    }

    GstWebRTCSessionDescription* answer =
        gst_webrtc_session_description_new(
            GST_WEBRTC_SDP_TYPE_ANSWER,
            sdp_msg);

    if (!answer) {
        std::cerr << "[" << cfg_.label
                  << "] Failed to create WebRTC answer description\n";
        failed_ = true;
        gst_sdp_message_free(sdp_msg);
        return;
    }

    GstPromise* remote_promise = gst_promise_new();
    g_signal_emit_by_name(
        webrtcbin_,
        "set-remote-description",
        answer,
        remote_promise);
    gst_promise_interrupt(remote_promise);
    gst_promise_unref(remote_promise);

    gst_webrtc_session_description_free(answer);
}

bool WebRtcBinStream::do_whep_exchange(
    const std::string& offer_sdp,
    std::string& out_answer_sdp) {
    CURL* curl = curl_easy_init();

    if (!curl) {
        std::cerr << "[" << cfg_.label << "] curl_easy_init failed\n";
        return false;
    }

    std::string response;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/sdp");
    headers = curl_slist_append(headers, "Accept: application/sdp");

    curl_easy_setopt(curl, CURLOPT_URL, cfg_.url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, offer_sdp.c_str());
    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDSIZE,
        static_cast<long>(offer_sdp.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[" << cfg_.label
                  << "] curl error: " << curl_easy_strerror(res) << "\n";
        return false;
    }

    if (http_code != 200 && http_code != 201) {
        std::cerr << "[" << cfg_.label << "] WHEP HTTP " << http_code
                  << "\n"
                  << response << "\n";
        return false;
    }

    if (response.empty()) {
        std::cerr << "[" << cfg_.label
                  << "] WHEP response body was empty\n";
        return false;
    }

    out_answer_sdp = std::move(response);
    return true;
}

void WebRtcBinStream::on_ice_candidate(
    guint mline_index,
    gchararray candidate) {
    if (stopping_) {
        return;
    }

    // This backend currently performs only the initial POST offer/answer
    // exchange. Full WHEP trickle ICE would PATCH these candidates back to the
    // WHEP resource URL returned by the server.
    std::cout << "[" << cfg_.label << "] ICE [" << mline_index
              << "]: " << candidate << "\n";
}

void WebRtcBinStream::on_pad_added(GstPad* pad) {
    if (stopping_ || !pipeline_) {
        return;
    }

    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) {
        caps = gst_pad_query_caps(pad, nullptr);
    }

    if (!caps || gst_caps_is_empty(caps)) {
        if (caps) {
            gst_caps_unref(caps);
        }

        std::cerr << "[" << cfg_.label
                  << "] webrtcbin pad added without usable caps\n";
        return;
    }

    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(structure, "media");

    if (!media) {
        gst_caps_unref(caps);
        return;
    }

    const bool is_video = std::strcmp(media, "video") == 0;
    const bool is_audio = std::strcmp(media, "audio") == 0;

    gst_caps_unref(caps);

    if (is_video) {
        if (video_linked_) {
            return;
        }

        video_linked_ = true;

        GstElement* jitterbuffer =
            gst_element_factory_make("rtpjitterbuffer", nullptr);
        GstElement* depay = gst_element_factory_make("rtph264depay", nullptr);
        GstElement* parse = gst_element_factory_make("h264parse", nullptr);
        GstElement* decode = gst_element_factory_make("vtdec_hw", nullptr);

        if (!decode) {
            decode = gst_element_factory_make("vtdec", nullptr);
        }

        if (!decode) {
            decode = gst_element_factory_make("avdec_h264", nullptr);
        }

        GstElement* convert = gst_element_factory_make("videoconvert", nullptr);
        GstElement* capsfilter =
            gst_element_factory_make("capsfilter", nullptr);
        GstElement* queue = gst_element_factory_make("queue", nullptr);
        GstElement* appsink = gst_element_factory_make("appsink", nullptr);

        if (!jitterbuffer || !depay || !parse || !decode || !convert ||
            !capsfilter || !queue || !appsink) {
            std::cerr << "[" << cfg_.label
                      << "] Failed to create video receive chain\n";
            failed_ = true;
            return;
        }

        g_object_set(jitterbuffer, "latency", (guint)0, nullptr);

        g_object_set(
            queue,
            "max-size-buffers",
            (guint)2,
            "leaky",
            (guint)2,
            nullptr);

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
            appsink,
            "new-sample",
            G_CALLBACK(on_new_sample),
            this);

        gst_bin_add_many(
            GST_BIN(pipeline_),
            jitterbuffer,
            depay,
            parse,
            decode,
            convert,
            capsfilter,
            queue,
            appsink,
            nullptr);

        if (!gst_element_link_many(
                jitterbuffer,
                depay,
                parse,
                decode,
                convert,
                capsfilter,
                queue,
                appsink,
                nullptr)) {
            std::cerr << "[" << cfg_.label
                      << "] Failed to link video receive chain\n";
            failed_ = true;
            return;
        }

        gst_element_sync_state_with_parent(jitterbuffer);
        gst_element_sync_state_with_parent(depay);
        gst_element_sync_state_with_parent(parse);
        gst_element_sync_state_with_parent(decode);
        gst_element_sync_state_with_parent(convert);
        gst_element_sync_state_with_parent(capsfilter);
        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(appsink);

        GstPad* sink_pad = gst_element_get_static_pad(jitterbuffer, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, sink_pad);
        gst_object_unref(sink_pad);

        if (ret != GST_PAD_LINK_OK) {
            std::cerr << "[" << cfg_.label
                      << "] Failed to link WebRTC video pad: " << ret << "\n";
            failed_ = true;
        }

    } else if (is_audio) {
        if (audio_linked_) {
            return;
        }

        audio_linked_ = true;

        GstElement* queue = gst_element_factory_make("queue", nullptr);
        GstElement* jitterbuffer =
            gst_element_factory_make("rtpjitterbuffer", nullptr);
        GstElement* depay = gst_element_factory_make("rtpopusdepay", nullptr);
        GstElement* decode = gst_element_factory_make("opusdec", nullptr);
        GstElement* convert = gst_element_factory_make("audioconvert", nullptr);
        GstElement* sink = gst_element_factory_make("fakesink", nullptr);

        if (!queue || !jitterbuffer || !depay || !decode || !convert ||
            !sink) {
            std::cerr << "[" << cfg_.label
                      << "] Failed to create audio receive chain\n";
            failed_ = true;
            return;
        }

        g_object_set(
            queue,
            "max-size-buffers",
            (guint)2,
            "leaky",
            (guint)2,
            nullptr);

        g_object_set(jitterbuffer, "latency", (guint)0, nullptr);

        g_object_set(
            sink,
            "sync",
            FALSE,
            "async",
            FALSE,
            nullptr);

        gst_bin_add_many(
            GST_BIN(pipeline_),
            queue,
            jitterbuffer,
            depay,
            decode,
            convert,
            sink,
            nullptr);

        if (!gst_element_link_many(
                queue,
                jitterbuffer,
                depay,
                decode,
                convert,
                sink,
                nullptr)) {
            std::cerr << "[" << cfg_.label
                      << "] Failed to link audio receive chain\n";
            failed_ = true;
            return;
        }

        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(jitterbuffer);
        gst_element_sync_state_with_parent(depay);
        gst_element_sync_state_with_parent(decode);
        gst_element_sync_state_with_parent(convert);
        gst_element_sync_state_with_parent(sink);

        GstPad* sink_pad = gst_element_get_static_pad(queue, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, sink_pad);
        gst_object_unref(sink_pad);

        if (ret != GST_PAD_LINK_OK) {
            std::cerr << "[" << cfg_.label
                      << "] Failed to link WebRTC audio pad: " << ret << "\n";
            failed_ = true;
        }
    }
}

GstFlowReturn WebRtcBinStream::on_new_sample(
    GstElement* sink,
    gpointer data) {
    auto* self = static_cast<WebRtcBinStream*>(data);

    if (!self || self->stopping_) {
        return GST_FLOW_EOS;
    }

    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));

    if (!sample) {
        return GST_FLOW_ERROR;
    }

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
            std::memcpy(
                f.y_plane.data() + static_cast<size_t>(row) * w,
                y_src + static_cast<size_t>(row) * y_stride,
                static_cast<size_t>(w));
        }

        const int uv_rows = h / 2;
        f.uv_plane.resize(static_cast<size_t>(w) * uv_rows);

        for (int row = 0; row < uv_rows; ++row) {
            std::memcpy(
                f.uv_plane.data() + static_cast<size_t>(row) * w,
                uv_src + static_cast<size_t>(row) * uv_stride,
                static_cast<size_t>(w));
        }


        f.dirty = true;

        gst_video_frame_unmap(&vframe);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}
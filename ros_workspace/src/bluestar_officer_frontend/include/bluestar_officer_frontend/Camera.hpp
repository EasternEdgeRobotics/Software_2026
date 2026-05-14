#ifndef CAMERAS_HPP
#define CAMERAS_HPP

#include <glad/glad.h>
#include <imgui.h>

#include <chrono>
#include <memory>
#include <string>
#include <future>
#include <vector>
#include <deque>
#include <cstdint>

class CameraStream;

class Camera {
public:
    Camera(
        char (&urlRef)[512], 
        char (&videoCapsRef)[1024],
        char (&audioCapsRef)[1024],
        unsigned int fallback, 
        int cameraNumber);
    ~Camera();

    void start();
    void stop();
    void render(const ImVec2& size);

    void flip_vertically();
    void flip_horizontally();
    bool screenshot();
    void setScreenshotSuffix(const std::string& suffix);
    void setScreenshotCrop(
        bool enabled,
        float left,
        float right,
        float top,
        float bottom);

private:
    void syncStream();
    void uploadFrame();
    void ensureFbo(int width, int height);
    void destroyGlResources();
    bool saveScreenshot();
    void drainScreenshotWrites(bool wait);
    void drawScreenshotCropOverlay(const ImVec2& imageMin, const ImVec2& imageMax) const;

    char (&urlPtr)[512];
    char (&videoCapsPtr)[1024];
    char (&audioCapsPtr)[1024];
    unsigned int fallback;
    std::string label;

    std::unique_ptr<CameraStream> stream;
    std::string activeUrl;
    std::string activeVideoCaps;
    std::string activeAudioCaps;
    bool running = false;

    bool flip_frame_vertically = false;
    bool flip_frame_horizontally = false;
    bool take_screenshot = false;

    std::string screenshotSuffix = "section_0";
    
    GLuint texY = 0;
    GLuint texUV = 0;
    GLuint fbo = 0;
    GLuint fboTex = 0;

    int frameWidth = 0;
    int frameHeight = 0;
    int fboWidth = 0;
    int fboHeight = 0;

    bool screenshotCropEnabled = false;
    float screenshotCropLeft = 0.0f;
    float screenshotCropRight = 0.0f;
    float screenshotCropTop = 0.0f;
    float screenshotCropBottom = 0.0f;

    bool hasReceivedFrame = false;
    std::chrono::steady_clock::time_point lastFrameTime;
    std::chrono::steady_clock::time_point streamStartTime;
    std::chrono::steady_clock::time_point lastReconnectAttempt;
    std::vector<std::future<bool>> screenshotWrites;
};

#endif
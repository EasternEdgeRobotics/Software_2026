#ifndef CAMERAS_HPP
#define CAMERAS_HPP

#include <glad/glad.h>
#include <imgui.h>

#include <chrono>
#include <memory>
#include <string>

class WebRTCStream;

class Camera {
public:
    Camera(char (&urlRef)[512], unsigned int fallback);
    ~Camera();

    void start();
    void stop();
    void render(const ImVec2& size);

    void flip_vertically();
    void flip_horizontally();
    bool screenshot();

private:
    void syncStream();
    void uploadFrame();
    void ensureFbo(int width, int height);
    void destroyGlResources();
    bool saveScreenshot();

    char (&urlPtr)[512];
    unsigned int fallback;

    std::unique_ptr<WebRTCStream> stream;
    std::string activeUrl;
    bool running = false;

    bool flip_frame_vertically = false;
    bool flip_frame_horizontally = false;
    bool take_screenshot = false;

    GLuint texY = 0;
    GLuint texUV = 0;
    GLuint fbo = 0;
    GLuint fboTex = 0;

    int frameWidth = 0;
    int frameHeight = 0;
    int fboWidth = 0;
    int fboHeight = 0;

    std::chrono::steady_clock::time_point lastFrameTime;
};

#endif
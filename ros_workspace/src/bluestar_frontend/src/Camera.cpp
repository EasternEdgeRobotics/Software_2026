#include "Camera.hpp"
#include "webrtc_stream.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

static const char* kVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* kFragSrc = R"(
#version 330 core
in vec2 v_uv;
out vec4 frag_color;
uniform sampler2D tex_y;
uniform sampler2D tex_uv;
void main() {
    float y = texture(tex_y, v_uv).r;
    vec2 uv = texture(tex_uv, v_uv).rg;
    float cb = uv.x - 0.5;
    float cr = uv.y - 0.5;
    float r = y + 1.402 * cr;
    float g = y - 0.34414 * cb - 0.71414 * cr;
    float b = y + 1.772 * cb;
    frag_color = vec4(clamp(vec3(r, g, b), 0.0, 1.0), 1.0);
}
)";

struct FullscreenQuad {
    GLuint vao = 0;
    GLuint vbo = 0;

    void init() {
        static const float verts[] = {
            -1.0f, +1.0f, 0.0f, 0.0f,
            +1.0f, +1.0f, 1.0f, 0.0f,
            +1.0f, -1.0f, 1.0f, 1.0f,
            -1.0f, +1.0f, 0.0f, 0.0f,
            +1.0f, -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 1.0f,
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            2,
            GL_FLOAT,
            GL_FALSE,
            4 * sizeof(float),
            (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            4 * sizeof(float),
            (void*)(2 * sizeof(float)));

        glBindVertexArray(0);
    }

    void destroy() {
        if (vbo) {
            glDeleteBuffers(1, &vbo);
            vbo = 0;
        }
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
    }
};

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetShaderInfoLog(shader, sizeof(buf), nullptr, buf);
        std::cerr << "Shader compile error: " << buf << std::endl;
    }

    return shader;
}

GLuint buildShader() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetProgramInfoLog(program, sizeof(buf), nullptr, buf);
        std::cerr << "Shader link error: " << buf << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

std::mutex gGstMutex;
int gGstUsers = 0;
GMainContext* gGstContext = nullptr;
GMainLoop* gMainLoop = nullptr;
std::thread gMainLoopThread;

std::mutex gGlMutex;
int gGlUsers = 0;
GLuint gNv12Shader = 0;
FullscreenQuad gQuad;

void acquireGst() {
    std::lock_guard<std::mutex> lock(gGstMutex);

    if (gGstUsers++ != 0) {
        return;
    }

    gst_init(nullptr, nullptr);

    gGstContext = g_main_context_new();
    gMainLoop = g_main_loop_new(gGstContext, FALSE);

    gMainLoopThread = std::thread([] {
        g_main_context_push_thread_default(gGstContext);
        g_main_loop_run(gMainLoop);
        g_main_context_pop_thread_default(gGstContext);
    });
}

void releaseGst() {
    std::lock_guard<std::mutex> lock(gGstMutex);

    if (--gGstUsers != 0) {
        return;
    }

    if (gMainLoop) {
        g_main_loop_quit(gMainLoop);
    }

    if (gMainLoopThread.joinable()) {
        gMainLoopThread.join();
    }

    if (gMainLoop) {
        g_main_loop_unref(gMainLoop);
        gMainLoop = nullptr;
    }

    if (gGstContext) {
        g_main_context_unref(gGstContext);
        gGstContext = nullptr;
    }
}

void acquireSharedGl() {
    std::lock_guard<std::mutex> lock(gGlMutex);

    if (gGlUsers++ == 0) {
        gNv12Shader = buildShader();
        gQuad.init();
    }
}

void releaseSharedGl() {
    std::lock_guard<std::mutex> lock(gGlMutex);

    --gGlUsers;
    if (gGlUsers == 0) {
        if (glfwGetCurrentContext()) {
            gQuad.destroy();
            if (gNv12Shader) {
                glDeleteProgram(gNv12Shader);
                gNv12Shader = 0;
            }
        }
    }
}

void ensurePlaneTex(
    GLuint& tex,
    GLenum format,
    int width,
    int height,
    const uint8_t* data)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (tex == 0) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            static_cast<GLint>(format),
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            data);
    } else {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            width,
            height,
            format,
            GL_UNSIGNED_BYTE,
            data);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void flipBufferVertically(
    std::vector<uint8_t>& pixels,
    int width,
    int height,
    int channels)
{
    const int stride = width * channels;
    std::vector<uint8_t> row(stride);

    for (int y = 0; y < height / 2; ++y) {
        uint8_t* top = pixels.data() + y * stride;
        uint8_t* bottom = pixels.data() + (height - 1 - y) * stride;

        std::memcpy(row.data(), top, stride);
        std::memcpy(top, bottom, stride);
        std::memcpy(bottom, row.data(), stride);
    }
}

void flipBufferHorizontally(
    std::vector<uint8_t>& pixels,
    int width,
    int height,
    int channels)
{
    const int stride = width * channels;

    for (int y = 0; y < height; ++y) {
        uint8_t* row = pixels.data() + y * stride;
        for (int x = 0; x < width / 2; ++x) {
            uint8_t* left = row + x * channels;
            uint8_t* right = row + (width - 1 - x) * channels;

            for (int c = 0; c < channels; ++c) {
                std::swap(left[c], right[c]);
            }
        }
    }
}

} // namespace

Camera::Camera(char (&urlRef)[512], unsigned int fallback)
    : urlPtr(urlRef),
      fallback(fallback),
      lastFrameTime(std::chrono::steady_clock::now()) {}

Camera::~Camera() {
    stop();

    if (glfwGetCurrentContext()) {
        destroyGlResources();
    }
}

void Camera::start() {
    if (running) return;

    acquireGst();
    acquireSharedGl();

    running = true;
    syncStream();
}

void Camera::stop() {
    if (!running) return;

    stream.reset();
    activeUrl.clear();
    running = false;

    releaseSharedGl();
    releaseGst();
}

void Camera::syncStream() {
    if (!running) return;

    const size_t len = strnlen(urlPtr, sizeof(urlPtr));
    std::string desiredUrl(urlPtr, len);

    if (desiredUrl == activeUrl) {
        return;
    }

    stream.reset();
    activeUrl.clear();

    if (desiredUrl.empty()) {
        return;
    }

    auto next = std::make_unique<WebRTCStream>();

    StreamConfig cfg;
    cfg.whep_url = desiredUrl;
    cfg.label = "Camera";

    if (!next->start(cfg, gGstContext)) {
        std::cerr << "Failed to start WebRTC stream: " << desiredUrl
                << std::endl;
        return;
    }

    stream = std::move(next);
    activeUrl = desiredUrl;
}

void Camera::ensureFbo(int width, int height) {
    if (fbo != 0 && fboWidth == width && fboHeight == height) {
        return;
    }

    if (fbo != 0) {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }

    if (fboTex != 0) {
        glDeleteTextures(1, &fboTex);
        fboTex = 0;
    }

    glGenTextures(1, &fboTex);
    glBindTexture(GL_TEXTURE_2D, fboTex);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        fboTex,
        0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Camera FBO incomplete" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    fboWidth = width;
    fboHeight = height;
}

void Camera::uploadFrame() {
    if (!stream) return;

    FrameData frame;
    if (!stream->poll_frame(frame)) {
        return;
    }

    if (frame.width <= 0 || frame.height <= 0) {
        return;
    }

    if (frame.width != frameWidth || frame.height != frameHeight) {
        if (texY) {
            glDeleteTextures(1, &texY);
            texY = 0;
        }
        if (texUV) {
            glDeleteTextures(1, &texUV);
            texUV = 0;
        }
        frameWidth = frame.width;
        frameHeight = frame.height;
    }

    ensurePlaneTex(
        texY,
        GL_RED,
        frame.width,
        frame.height,
        frame.y_plane.data());

    ensurePlaneTex(
        texUV,
        GL_RG,
        frame.width / 2,
        frame.height / 2,
        frame.uv_plane.data());

    ensureFbo(frame.width, frame.height);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, frame.width, frame.height);

    glUseProgram(gNv12Shader);
    glUniform1i(glGetUniformLocation(gNv12Shader, "tex_y"), 0);
    glUniform1i(glGetUniformLocation(gNv12Shader, "tex_uv"), 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texY);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texUV);

    glBindVertexArray(gQuad.vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    lastFrameTime = std::chrono::steady_clock::now();

    if (take_screenshot) {
        take_screenshot = false;
        if (!saveScreenshot()) {
            std::cerr << "Failed to save screenshot" << std::endl;
        }
    }
}

void Camera::render(const ImVec2& size) {
    syncStream();
    uploadFrame();

    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - lastFrameTime)
                         .count();

    if (fboTex != 0 && elapsedMs <= 3000) {
        float u0 = flip_frame_horizontally ? 1.0f : 0.0f;
        float u1 = flip_frame_horizontally ? 0.0f : 1.0f;

        float v0 = flip_frame_vertically ? 0.0f : 1.0f;
        float v1 = flip_frame_vertically ? 1.0f : 0.0f;

        ImGui::Image(
            (ImTextureID)(uintptr_t)fboTex,
            size,
            ImVec2(u0, v0),
            ImVec2(u1, v1));
    } else {
        ImGui::Image((ImTextureID)(uintptr_t)fallback, size);
    }
}

void Camera::flip_vertically() {
    flip_frame_vertically = !flip_frame_vertically;
}

void Camera::flip_horizontally() {
    flip_frame_horizontally = !flip_frame_horizontally;
}

bool Camera::screenshot() {
    const char* home = std::getenv("HOME");
    if (!home) return false;

    fs::path dir = fs::path(home) / "Pictures";
    if (!fs::exists(dir)) return false;

    take_screenshot = true;
    return true;
}

bool Camera::saveScreenshot() {
    if (fbo == 0 || fboWidth <= 0 || fboHeight <= 0) {
        return false;
    }

    const char* home = std::getenv("HOME");
    if (!home) return false;

    fs::path dir = fs::path(home) / "Pictures";
    if (!fs::exists(dir)) return false;

    fs::path file =
        dir /
        ("screenshot_" +
         std::to_string(
             std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count()) +
         ".png");

    std::vector<uint8_t> pixels(
        static_cast<size_t>(fboWidth) * fboHeight * 4);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadPixels(
        0,
        0,
        fboWidth,
        fboHeight,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    flipBufferVertically(pixels, fboWidth, fboHeight, 4);

    if (flip_frame_vertically) {
        flipBufferVertically(pixels, fboWidth, fboHeight, 4);
    }

    if (flip_frame_horizontally) {
        flipBufferHorizontally(pixels, fboWidth, fboHeight, 4);
    }

    return stbi_write_png(
               file.string().c_str(),
               fboWidth,
               fboHeight,
               4,
               pixels.data(),
               fboWidth * 4) != 0;
}

void Camera::destroyGlResources() {
    if (texY) {
        glDeleteTextures(1, &texY);
        texY = 0;
    }
    if (texUV) {
        glDeleteTextures(1, &texUV);
        texUV = 0;
    }
    if (fboTex) {
        glDeleteTextures(1, &fboTex);
        fboTex = 0;
    }
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
}
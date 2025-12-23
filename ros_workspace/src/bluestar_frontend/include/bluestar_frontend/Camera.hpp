#ifndef CAMERAS_HPP
#define CAMERAS_HPP

#include <opencv2/opencv.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <memory>
#include <string>
#include <chrono>

class Camera {
public:
    Camera(char (&urlRef)[64], unsigned int fallback)
        : urlPtr(urlRef), running(false), fallback(fallback), texture(0), lastFrameTime(std::chrono::steady_clock::now()) {}

    void start() {
        running = true;
        captureThread = std::thread(&Camera::captureFrames, this);
    }

    void stop() {
        running = false;
        if (captureThread.joinable()) captureThread.join();
    }

    void render(const ImVec2& size) {
        cv::Mat frame;
        bool frameCaptured = false;

        {
            std::lock_guard<std::mutex> lock(frameMutex);
            if (!frameQueue.empty()) {
                frame = frameQueue.front();
                frameQueue.pop();

                if (flip_frame_vertically && flip_frame_horizontally)
                {
                    cv::flip(frame, frame, -1);
                }
                
                else
                {
                    if (flip_frame_vertically)
                    {
                        cv::flip(frame, frame, 0);
                    }
    
                    if (flip_frame_horizontally)
                    {
                        cv::flip(frame, frame, 1);
                    }

                }

                if (take_screenshot)
                {
                    take_screenshot = false;
                    if (filesystem::exists(std::string(getenv("HOME")) + "/Pictures/"))
                    {
                        std::string filename = std::string(getenv("HOME")) + "/Pictures/screenshot_" +
                            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count()) + ".png";
                        cv::imwrite(filename, frame);
                    }
                }
                
                frameCaptured = true;
                lastFrameTime = std::chrono::steady_clock::now();
            }
        }

        if (frameCaptured) {
            uploadToTexture(frame);
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime).count();

        if (elapsed > 3000 && texture) {
            glDeleteTextures(1, &texture);
            texture = 0;
        }
        
        // Since we called ImGui::SetCursorPos before this method, calling ImGui::Image will cause the image to render in the desired position
        if (texture) {
            ImGui::Image((ImTextureID)(uintptr_t)texture, size);
        } else {
            ImGui::Image((ImTextureID)(intptr_t)fallback, size);
        }
    }

    void flip_vertically()
    {
        flip_frame_vertically = !flip_frame_vertically;
    }

    void flip_horizontally()
    {
        flip_frame_horizontally = !flip_frame_horizontally;
    }

    bool screenshot()
    {
        if (!filesystem::exists(std::string(getenv("HOME")) + "/Pictures/")) return false;
        take_screenshot = true;
        return true;
    }

    ~Camera() {
        stop();
        if (texture) glDeleteTextures(1, &texture);
    }

private:
    char (&urlPtr)[64];
    std::thread captureThread;
    std::mutex frameMutex;
    std::queue<cv::Mat> frameQueue;
    std::atomic<bool> running;
    unsigned int fallback;
    GLuint texture;
    int displayWidth = 640;
    int displayHeight = 480;
    bool flip_frame_vertically = false;
    bool flip_frame_horizontally = false;
    bool take_screenshot = false;

    std::chrono::steady_clock::time_point lastFrameTime;

    void captureFrames() {
        while (running) {
            if (strlen(urlPtr) == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
    
            cv::VideoCapture cap;
            bool connected = false;
    
            while (running) {
                cap.release();  // Release any previous connection immediately
                cap.open(urlPtr, cv::CAP_FFMPEG);
    
                if (cap.isOpened()) {
                    std::cout << "Camera connected: " << urlPtr << std::endl;
                    connected = true;
    
                    // Reduce buffering and improve FPS
                    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
                    cap.set(cv::CAP_PROP_FPS, 60);
                    cap.set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 1000);
    
                    while (running && connected && strlen(urlPtr) > 0) {
                        cv::Mat frame;
                        if (!cap.read(frame) || frame.empty()) {
                            std::cerr << "Stream disconnected: " << urlPtr << std::endl;
                            connected = false;
                            break;
                        }
    
                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            if (frameQueue.size() < 5) {
                                frameQueue.push(frame.clone());
                            }
                        }
    
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Slight delay for CPU efficiency
                    }
                } else {
                    std::cerr << "Error: Could not open camera stream: " << urlPtr << ". Retrying in 100ms..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }
    }

    void uploadToTexture(const cv::Mat& frame) {
        if (texture) glDeleteTextures(1, &texture);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.cols, frame.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, frame.data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        displayWidth = frame.cols;
        displayHeight = frame.rows;
    }
};

#endif
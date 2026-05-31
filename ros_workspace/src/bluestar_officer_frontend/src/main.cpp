#include "Image.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <nlohmann/json.hpp>

#include "Config.hpp"
#include "Camera.hpp"

#include <iostream>
#include <cstdint>
#include <memory>

#include "images/logo.h"
#include "images/nosignal.h"

#include "streaming/CameraStreamFactory.hpp"

#include <optional>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <future>
#include <cstdio>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

BlueStarConfig bluestar_config;

bool showConfigWindow = false;
bool showCameraWindow = true;



bool flipCam1VerticallyButtonPressedLatch = false;
bool flipCam2VerticallyButtonPressedLatch = false;
bool flipCam3VerticallyButtonPressedLatch = false;
bool flipCam4VerticallyButtonPressedLatch = false;

bool flipCam1HorizontallyButtonPressedLatch = false;
bool flipCam2HorizontallyButtonPressedLatch = false;
bool flipCam3HorizontallyButtonPressedLatch = false;
bool flipCam4HorizontallyButtonPressedLatch = false;

bool cam1ScreenshotButtonPressedLatch = false;
bool cam2ScreenshotButtonPressedLatch = false;
bool cam3ScreenshotButtonPressedLatch = false;
bool cam4ScreenshotButtonPressedLatch = false;

bool camIncrementSectionPressedLatch = false;
int camScreenshotSection = 0;

bool cam1ScreenshotCropButtonPressedLatch = false;
bool cam2ScreenshotCropButtonPressedLatch = false;
bool cam3ScreenshotCropButtonPressedLatch = false;
bool cam4ScreenshotCropButtonPressedLatch = false;

bool cam1ScreenshotCropEnabled = false;
float cam1ScreenshotCropLeft = 0.0f;
float cam1ScreenshotCropRight = 0.0f;
float cam1ScreenshotCropTop = 0.0f;
float cam1ScreenshotCropBottom = 0.0f;

bool cam2ScreenshotCropEnabled = false;
float cam2ScreenshotCropLeft = 0.0f;
float cam2ScreenshotCropRight = 0.0f;
float cam2ScreenshotCropTop = 0.0f;
float cam2ScreenshotCropBottom = 0.0f;

bool cam3ScreenshotCropEnabled = false;
float cam3ScreenshotCropLeft = 0.0f;
float cam3ScreenshotCropRight = 0.0f;
float cam3ScreenshotCropTop = 0.0f;
float cam3ScreenshotCropBottom = 0.0f;

bool cam4ScreenshotCropEnabled = false;
float cam4ScreenshotCropLeft = 0.0f;
float cam4ScreenshotCropRight = 0.0f;
float cam4ScreenshotCropTop = 0.0f;
float cam4ScreenshotCropBottom = 0.0f;

bool camWebODMUploadPressedLatch = false;
int camSectionTotalPhotos = 0;

const char* app_id = "EasternEdge.BlueStar.OfficerFrontend";

std::string currentScreenshotSectionName;
fs::path currentScreenshotSectionDir;

void normalizeScreenshotCrop(
    float& left,
    float& right,
    float& top,
    float& bottom)
{
    left = std::clamp(left, 0.0f, 0.95f);
    right = std::clamp(right, 0.0f, 0.95f);
    top = std::clamp(top, 0.0f, 0.95f);
    bottom = std::clamp(bottom, 0.0f, 0.95f);

    const float horizontalCrop = left + right;
    const float verticalCrop = top + bottom;

    if (horizontalCrop > 0.95f) {
        const float scale = 0.95f / horizontalCrop;
        left *= scale;
        right *= scale;
    }

    if (verticalCrop > 0.95f) {
        const float scale = 0.95f / verticalCrop;
        top *= scale;
        bottom *= scale;
    }
}

std::string makeLocalTimestampForPath() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm localTime {};
#if defined(_WIN32)
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S");
    return stream.str();
}

fs::path defaultCaptureRoot() {
    const char* home = std::getenv("HOME");

    if (!home) {
        return fs::path("bluestar_captures");
    }

    return fs::path(home) / "Pictures" / "bluestar_captures";
}

fs::path defaultConfigPath() {
    const char* configPath = std::getenv("BLUESTAR_CONFIG_PATH");

    if (configPath && configPath[0] != '\0') {
        return fs::path(configPath);
    }

    const char* home = std::getenv("HOME");

    if (!home || home[0] == '\0') {
        return fs::path("bluestar_config.json");
    }

#if defined(__APPLE__)
    return fs::path(home) / "Library" / "Application Support" / "BlueStar" /
           "bluestar_config.json";
#else
    return fs::path(home) / ".config" / "bluestar" / "bluestar_config.json";
#endif
}

template <size_t N>
void copyJsonString(const json& data, const char* key, char (&dest)[N]) {
    if (!data.contains(key) || data[key].is_null()) {
        return;
    }

    if (!data[key].is_string()) {
        std::cerr << "Config key is not a string: " << key << std::endl;
        return;
    }

    std::snprintf(dest, N, "%s", data[key].get<std::string>().c_str());
}

json bluestarConfigToJson(const BlueStarConfig& config) {
    return json{
        {"cam1ip", config.cam1ip},
        {"cam2ip", config.cam2ip},
        {"cam3ip", config.cam3ip},
        {"cam4ip", config.cam4ip},

        {"cam1_video_caps", config.cam1_video_caps},
        {"cam1_audio_caps", config.cam1_audio_caps},

        {"cam2_video_caps", config.cam2_video_caps},
        {"cam2_audio_caps", config.cam2_audio_caps},

        {"cam3_video_caps", config.cam3_video_caps},
        {"cam3_audio_caps", config.cam3_audio_caps},

        {"cam4_video_caps", config.cam4_video_caps},
        {"cam4_audio_caps", config.cam4_audio_caps},
    };
}

bool saveBluestarConfigToFile(
    const fs::path& path,
    const BlueStarConfig& config)
{
    std::error_code ec;

    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);

        if (ec) {
            std::cerr << "Failed to create config directory: "
                      << path.parent_path() << std::endl;
            return false;
        }
    }

    std::ofstream output(path);

    if (!output) {
        std::cerr << "Failed to open config file for writing: " << path
                  << std::endl;
        return false;
    }

    output << bluestarConfigToJson(config).dump(4) << std::endl;
    return true;
}

bool loadBluestarConfigFromFile(
    const fs::path& path,
    BlueStarConfig& config)
{
    if (!fs::exists(path)) {
        std::cerr << "Config file does not exist. Creating default config: "
                  << path << std::endl;

        saveBluestarConfigToFile(path, config);
        return false;
    }

    std::ifstream input(path);

    if (!input) {
        std::cerr << "Failed to open config file: " << path << std::endl;
        return false;
    }

    try {
        json data;
        input >> data;

        copyJsonString(data, "cam1ip", config.cam1ip);
        copyJsonString(data, "cam2ip", config.cam2ip);
        copyJsonString(data, "cam3ip", config.cam3ip);
        copyJsonString(data, "cam4ip", config.cam4ip);

        copyJsonString(data, "cam1_video_caps", config.cam1_video_caps);
        copyJsonString(data, "cam1_audio_caps", config.cam1_audio_caps);

        copyJsonString(data, "cam2_video_caps", config.cam2_video_caps);
        copyJsonString(data, "cam2_audio_caps", config.cam2_audio_caps);

        copyJsonString(data, "cam3_video_caps", config.cam3_video_caps);
        copyJsonString(data, "cam3_audio_caps", config.cam3_audio_caps);

        copyJsonString(data, "cam4_video_caps", config.cam4_video_caps);
        copyJsonString(data, "cam4_audio_caps", config.cam4_audio_caps);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse config file " << path << ": "
                  << e.what() << std::endl;
        return false;
    }

    return true;
}

std::string makeSectionName(int section) {
    return "section_" + std::to_string(section);
}

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";

    for (char c : value) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }

    quoted += "'";
    return quoted;
}

std::string defaultWebodmUploadScript() {
    const char* script = std::getenv("BLUESTAR_WEBODM_SCRIPT");

    if (script && script[0] != '\0') {
        return script;
    }

    const char* home = std::getenv("HOME");

    if (!home || home[0] == '\0') {
        return "webodm_upload.py";
    }

    return (fs::path(home) / "Software_2026" / "science_officer" / "coral_garden" / "webodm_upload.py" ).string();
}

int main() {
    //initialize glfw and imgui  
    if (!glfwInit()) return -1;

    // Gnome wont respect the icon or name of apps without an app id
    #if defined(GLFW_WAYLAND_APP_ID)
        glfwWindowHintString(GLFW_WAYLAND_APP_ID, app_id);
    #endif

    #if defined(GLFW_X11_CLASS_NAME)
        glfwWindowHintString(GLFW_X11_CLASS_NAME, app_id);
    #endif

    #if defined(GLFW_X11_INSTANCE_NAME)
        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "easternedge_bluestar_officer_frontend");
    #endif 

    #if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #endif

    glfwWindowHint(GLFW_MAXIMIZED, true);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Eastern Edge BlueStar Science Officer GUI", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // OpenGL info
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);

    // std::cerr << "GL_VERSION: "
    //         << (version ? reinterpret_cast<const char*>(version) : "null")
    //         << std::endl;
    // std::cerr << "GL_RENDERER: "
    //         << (renderer ? reinterpret_cast<const char*>(renderer) : "null")
    //         << std::endl;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);

    #if defined(__APPLE__)
    const char* glslVersion = "#version 410";
    #else
    const char* glslVersion = "#version 330";
    #endif

    ImGui_ImplOpenGL3_Init(glslVersion);

    const fs::path configPath = defaultConfigPath();

    std::string configStatus;

    if (loadBluestarConfigFromFile(configPath, bluestar_config)) {
        configStatus = "Config loaded";
        std::cout << "Loaded config file: " << configPath << std::endl;
    } else {
        configStatus = "Using default config";
        std::cout << "Using default config. Config path: " << configPath
                  << std::endl;
    }
    
    //create images
    unsigned int noSignal = loadEmbeddedTexture(nosignal_jpg, nosignal_jpg_len);

    Camera cam1(bluestar_config.cam1ip, bluestar_config.cam1_video_caps, bluestar_config.cam1_audio_caps, noSignal, 1);
    Camera cam2(bluestar_config.cam2ip, bluestar_config.cam2_video_caps, bluestar_config.cam2_audio_caps, noSignal, 2);
    Camera cam3(bluestar_config.cam3ip, bluestar_config.cam3_video_caps, bluestar_config.cam3_audio_caps, noSignal, 3);
    Camera cam4(bluestar_config.cam4ip, bluestar_config.cam4_video_caps, bluestar_config.cam4_audio_caps, noSignal, 4);

    const std::string captureSessionId = makeLocalTimestampForPath();

    const fs::path captureRoot = defaultCaptureRoot();
    const fs::path captureSessionDir = captureRoot / captureSessionId;

    std::error_code captureDirError;
    fs::create_directories(captureSessionDir, captureDirError);

    if (captureDirError) {
        std::cerr << "Failed to create capture session directory: "
                << captureSessionDir << std::endl;
    } else {
        std::cout << "Capture session directory: " << captureSessionDir
                << std::endl;
    }

    auto updateScreenshotSection = [&]() {
        const std::string sectionName = makeSectionName(camScreenshotSection);
        const fs::path sectionDir = captureSessionDir / sectionName;

        currentScreenshotSectionName = sectionName;
        currentScreenshotSectionDir = sectionDir;

        std::error_code sectionDirError;
        fs::create_directories(sectionDir, sectionDirError);

        if (sectionDirError) {
            std::cerr << "Failed to create section directory: " << sectionDir
                    << std::endl;
        }

        cam1.setScreenshotSuffix(sectionName);
        cam2.setScreenshotSuffix(sectionName);
        cam3.setScreenshotSuffix(sectionName);
        cam4.setScreenshotSuffix(sectionName);

        cam1.setScreenshotDirectory(sectionDir.string());
        cam2.setScreenshotDirectory(sectionDir.string());
        cam3.setScreenshotDirectory(sectionDir.string());
        cam4.setScreenshotDirectory(sectionDir.string());

        std::cout << "Screenshot section set to: " << sectionName << std::endl;
        std::cout << "Screenshot directory set to: " << sectionDir << std::endl;
    };

    updateScreenshotSection();

    auto updateScreenshotCrop = [&]() {
        cam1.setScreenshotCrop(
            cam1ScreenshotCropEnabled,
            cam1ScreenshotCropLeft,
            cam1ScreenshotCropRight,
            cam1ScreenshotCropTop,
            cam1ScreenshotCropBottom);

        cam2.setScreenshotCrop(
            cam2ScreenshotCropEnabled,
            cam2ScreenshotCropLeft,
            cam2ScreenshotCropRight,
            cam2ScreenshotCropTop,
            cam2ScreenshotCropBottom);

        cam3.setScreenshotCrop(
            cam3ScreenshotCropEnabled,
            cam3ScreenshotCropLeft,
            cam3ScreenshotCropRight,
            cam3ScreenshotCropTop,
            cam3ScreenshotCropBottom);

        cam4.setScreenshotCrop(
            cam4ScreenshotCropEnabled,
            cam4ScreenshotCropLeft,
            cam4ScreenshotCropRight,
            cam4ScreenshotCropTop,
            cam4ScreenshotCropBottom);
    };

    updateScreenshotCrop();

    std::future<int> webodmUploadFuture;
    std::string webodmUploadStatus = "WebODM: idle";

    auto pollWebodmUpload = [&]() {
        if (!webodmUploadFuture.valid()) {
            return false;
        }

        const auto status =
            webodmUploadFuture.wait_for(std::chrono::milliseconds(0));

        if (status != std::future_status::ready) {
            return true;
        }

        try {
            const int exitCode = webodmUploadFuture.get();

            if (exitCode == 0) {
                webodmUploadStatus = "WebODM: upload complete";
            } else {
                webodmUploadStatus =
                    "WebODM: upload failed, exit code " +
                    std::to_string(exitCode);
            }
        } catch (const std::exception& e) {
            webodmUploadStatus =
                std::string("WebODM: upload failed: ") + e.what();
        } catch (...) {
            webodmUploadStatus = "WebODM: upload failed with unknown error";
        }

        return false;
    };

    auto launchWebodmUpload = [&](const fs::path& sectionDir,
                                const std::string& sectionName) {
        if (webodmUploadFuture.valid() &&
            webodmUploadFuture.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready) {
            webodmUploadStatus = "WebODM: upload already running";
            return;
        }

        if (!fs::exists(sectionDir)) {
            webodmUploadStatus =
                "WebODM: section directory does not exist: " +
                sectionDir.string();
            return;
        }

        const std::string scriptPath = defaultWebodmUploadScript();

        const std::string command =
            "python3 " + shellQuote(scriptPath) +
            " --section " + shellQuote(sectionName) +
            " --image-dir " + shellQuote(sectionDir.string());

        std::cout << "Launching WebODM upload: " << command << std::endl;

        webodmUploadStatus = "WebODM: uploading " + sectionName;

        webodmUploadFuture = std::async(
            std::launch::async,
            [command]() {
                return std::system(command.c_str());
            });
    };

    cam1.start();
    cam2.start();
    cam3.start();
    cam4.start();

    //render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const bool webodmUploadRunning = pollWebodmUpload();

        bool flipCam1VerticallyButtonPressed = false;
        bool flipCam2VerticallyButtonPressed = false;
        bool flipCam3VerticallyButtonPressed = false;
        bool flipCam4VerticallyButtonPressed = false;
        bool flipCam1HorizontallyButtonPressed = false;
        bool flipCam2HorizontallyButtonPressed = false;
        bool flipCam3HorizontallyButtonPressed = false;
        bool flipCam4HorizontallyButtonPressed = false;
        bool cam1ScreenshotButtonPressed = false;
        bool cam2ScreenshotButtonPressed = false;
        bool cam3ScreenshotButtonPressed = false;
        bool cam4ScreenshotButtonPressed = false;
        bool cam1ScreenshotCropButtonPressed = false;
        bool cam2ScreenshotCropButtonPressed = false;
        bool cam3ScreenshotCropButtonPressed = false;
        bool cam4ScreenshotCropButtonPressed = false;
        bool camIncrementSectionPressed = false;
        bool camWebODMUploadPressed = false;

        //top menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Windows")) {
                if (ImGui::MenuItem("Open Camera Window")) {
                    showCameraWindow = true;
                }
                if (ImGui::MenuItem("Open Config Window")) {
                    showConfigWindow = true;
                }          
                ImGui::EndMenu();
            }
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f); // Offset 
            if (ImGui::Button("Cam1 Screenshot")) {
                cam1ScreenshotButtonPressed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cam2 Screenshot")) {                
                cam2ScreenshotButtonPressed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cam3 Screenshot")) {                
                cam3ScreenshotButtonPressed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cam4 Screenshot")) {                
                cam4ScreenshotButtonPressed = true;
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
            if (ImGui::Button("Increment Section")) {                
                camIncrementSectionPressed = true;
            }

            ImGui::SameLine();
            ImGui::Text("Section: %d", camScreenshotSection);

            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
            if (webodmUploadRunning) {
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("Upload")) {
                camWebODMUploadPressed = true;
            }

            if (webodmUploadRunning) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(webodmUploadStatus.c_str());

            ImGui::SameLine(ImGui::GetWindowWidth() - 180);
            ImGui::Text("Photos: %d", camSectionTotalPhotos);
            
            //fps counter
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

            ImGui::EndMainMenuBar();
        }

        // Keyboard input handling
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) flipCam1VerticallyButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) flipCam2VerticallyButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) flipCam3VerticallyButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) flipCam4VerticallyButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) flipCam1HorizontallyButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) flipCam2HorizontallyButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) flipCam3HorizontallyButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) flipCam4HorizontallyButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam1ScreenshotButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam2ScreenshotButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam3ScreenshotButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) cam4ScreenshotButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) camIncrementSectionPressed = true;
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) cam1ScreenshotCropButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) cam2ScreenshotCropButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) cam3ScreenshotCropButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) cam4ScreenshotCropButtonPressed = true;
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) camIncrementSectionPressed = true;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) camWebODMUploadPressed = true;
        
        // We only want this input to be registered once per button hit 
        // rather than toggling every frame as long as button is pressed
        if (flipCam1VerticallyButtonPressed) {
            if (!flipCam1VerticallyButtonPressedLatch) cam1.flip_vertically();
            flipCam1VerticallyButtonPressedLatch = true;
        } else {
            flipCam1VerticallyButtonPressedLatch = false;
        }
        if (flipCam2VerticallyButtonPressed) {
            if (!flipCam2VerticallyButtonPressedLatch) cam2.flip_vertically();
            flipCam2VerticallyButtonPressedLatch = true;
        } else {
            flipCam2VerticallyButtonPressedLatch = false;
        }
        if (flipCam3VerticallyButtonPressed) {
            if (!flipCam3VerticallyButtonPressedLatch) cam3.flip_vertically();
            flipCam3VerticallyButtonPressedLatch = true;
        } else {
            flipCam3VerticallyButtonPressedLatch = false;
        }
        if (flipCam4VerticallyButtonPressed) {
            if (!flipCam4VerticallyButtonPressedLatch) cam4.flip_vertically();
            flipCam4VerticallyButtonPressedLatch = true;
        } else {
            flipCam4VerticallyButtonPressedLatch = false;
        }
        if (flipCam1HorizontallyButtonPressed) {
            if (!flipCam1HorizontallyButtonPressedLatch) cam1.flip_horizontally();
            flipCam1HorizontallyButtonPressedLatch = true;
        } else {
            flipCam1HorizontallyButtonPressedLatch = false;
        }
        if (flipCam2HorizontallyButtonPressed) {
            if (!flipCam2HorizontallyButtonPressedLatch) cam2.flip_horizontally();
            flipCam2HorizontallyButtonPressedLatch = true;
        } else {
            flipCam2HorizontallyButtonPressedLatch = false;
        }
        if (flipCam3HorizontallyButtonPressed) {
            if (!flipCam3HorizontallyButtonPressedLatch) cam3.flip_horizontally();
            flipCam3HorizontallyButtonPressedLatch = true;
        } else {
            flipCam3HorizontallyButtonPressedLatch = false;
        }
        if (flipCam4HorizontallyButtonPressed) {
            if (!flipCam4HorizontallyButtonPressedLatch) cam4.flip_horizontally();
            flipCam4HorizontallyButtonPressedLatch = true;
        } else {
            flipCam4HorizontallyButtonPressedLatch = false;
        }

        if (cam1ScreenshotButtonPressed) {
            if (!cam1ScreenshotButtonPressedLatch) {
                if (!cam1.screenshot()) {
                    std::cerr << "Failed to take screenshot for Cam1" << std::endl;
                } else {
                    ++camSectionTotalPhotos;
                }
            }
            cam1ScreenshotButtonPressedLatch = true;
        } else {
            cam1ScreenshotButtonPressedLatch = false;
        }

        if (cam2ScreenshotButtonPressed) {
            if (!cam2ScreenshotButtonPressedLatch) {
                if (!cam2.screenshot()) {
                    std::cerr << "Failed to take screenshot for Cam2" << std::endl;
                } else {
                    ++camSectionTotalPhotos;
                }
            }
            cam2ScreenshotButtonPressedLatch = true;
        } else {
            cam2ScreenshotButtonPressedLatch = false;
        }

        if (cam3ScreenshotButtonPressed) {
            if (!cam3ScreenshotButtonPressedLatch) {
                if (!cam3.screenshot()) {
                    std::cerr << "Failed to take screenshot for Cam3" << std::endl;
                } else {
                    ++camSectionTotalPhotos;
                }
            }
            cam3ScreenshotButtonPressedLatch = true;
        } else {
            cam3ScreenshotButtonPressedLatch = false;
        }

        if (cam4ScreenshotButtonPressed) {
            if (!cam4ScreenshotButtonPressedLatch) {
                if (!cam4.screenshot()) {
                    std::cerr << "Failed to take screenshot for Cam4" << std::endl;
                } else {
                    ++camSectionTotalPhotos;
                }
            }
            cam4ScreenshotButtonPressedLatch = true;
        } else {
            cam4ScreenshotButtonPressedLatch = false;
        }

        if (cam1ScreenshotCropButtonPressed) {
            if (!cam1ScreenshotCropButtonPressedLatch) cam1ScreenshotCropEnabled = !cam1ScreenshotCropEnabled;;
            normalizeScreenshotCrop(cam1ScreenshotCropLeft, cam1ScreenshotCropRight, cam1ScreenshotCropTop, cam1ScreenshotCropBottom);
            updateScreenshotCrop();
            cam1ScreenshotCropButtonPressedLatch = true;
        } else {
            cam1ScreenshotCropButtonPressedLatch = false;
        }

        if (cam2ScreenshotCropButtonPressed) {
            if (!cam2ScreenshotCropButtonPressedLatch) cam2ScreenshotCropEnabled = !cam2ScreenshotCropEnabled;;
            normalizeScreenshotCrop(cam2ScreenshotCropLeft, cam2ScreenshotCropRight, cam2ScreenshotCropTop, cam2ScreenshotCropBottom);
            updateScreenshotCrop();
            cam2ScreenshotCropButtonPressedLatch = true;
        } else {
            cam2ScreenshotCropButtonPressedLatch = false;
        }

        if (cam3ScreenshotCropButtonPressed) {
            if (!cam3ScreenshotCropButtonPressedLatch) cam3ScreenshotCropEnabled = !cam3ScreenshotCropEnabled;;
            normalizeScreenshotCrop(cam3ScreenshotCropLeft, cam3ScreenshotCropRight, cam3ScreenshotCropTop, cam3ScreenshotCropBottom);
            updateScreenshotCrop();
            cam3ScreenshotCropButtonPressedLatch = true;
        } else {
            cam3ScreenshotCropButtonPressedLatch = false;
        }

        if (cam4ScreenshotCropButtonPressed) {
            if (!cam4ScreenshotCropButtonPressedLatch) cam4ScreenshotCropEnabled = !cam4ScreenshotCropEnabled;;
            normalizeScreenshotCrop(cam4ScreenshotCropLeft, cam4ScreenshotCropRight, cam4ScreenshotCropTop, cam4ScreenshotCropBottom);
            updateScreenshotCrop();
            cam4ScreenshotCropButtonPressedLatch = true;
        } else {
            cam4ScreenshotCropButtonPressedLatch = false;
        }

        if (camIncrementSectionPressed) {
            if (!camIncrementSectionPressedLatch) {
                ++camScreenshotSection;
                camSectionTotalPhotos = 0;
                updateScreenshotSection();
            }
            camIncrementSectionPressedLatch = true;
        } else {
            camIncrementSectionPressedLatch = false;
        }

        if (camWebODMUploadPressed) {
            if (!camWebODMUploadPressedLatch) {
                cam1.waitForScreenshotWrites();
                cam2.waitForScreenshotWrites();
                cam3.waitForScreenshotWrites();
                cam4.waitForScreenshotWrites();

                launchWebodmUpload(
                    currentScreenshotSectionDir,
                    currentScreenshotSectionName);
            }
            camWebODMUploadPressedLatch = true;
        } else {
            camWebODMUploadPressedLatch = false;
        }
        
        // config window
        if (showConfigWindow) {
            ImGui::SetNextWindowSize(ImVec2(750, 400), ImGuiCond_FirstUseEver); // Prevents the window from not being resizeable
            ImGui::Begin("Config Editor", &showConfigWindow);
            if (ImGui::BeginTabBar("Config Tabs")) {
                if (ImGui::BeginTabItem("Cameras")) {


                    #if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
                        constexpr int cameraColumnCount = 4;
                    #else
                        constexpr int cameraColumnCount = 2;
                    #endif

                    ImGui::Text("URLs must include all elements. (ie http://192.168.137.200:8889/cam/whep)");

                    #if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
                        ImGui::Text("Video and audio caps can be left blank if they match defaults. (H264 w/ no audio)");
                    #endif
                    
                    ImGui::TextWrapped(
                        "Config file: %s",
                        configPath.string().c_str());

                    if (ImGui::Button("Save Camera Config")) {
                        if (saveBluestarConfigToFile(
                                configPath,
                                bluestar_config)) {
                            configStatus = "Config saved";
                        } else {
                            configStatus = "Failed to save config";
                        }
                    }

                    ImGui::SameLine();
                    ImGui::TextUnformatted(configStatus.c_str());

                    ImGui::Separator();

                    if (ImGui::BeginTable("Camera Table", cameraColumnCount, ImGuiTableFlags_Borders |
                                ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_SizingStretchSame |
                                ImGuiTableFlags_Resizable)) {

                        ImGui::TableSetupColumn("Number", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                        ImGui::TableSetupColumn("URL", ImGuiTableColumnFlags_WidthStretch);

                        #if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
                            ImGui::TableSetupColumn(
                                "Video Caps",
                                ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableSetupColumn(
                                "Audio Caps",
                                ImGuiTableColumnFlags_WidthStretch);
                        #endif

                        ImGui::TableHeadersRow();

                        char* cameraUrls[] = {
                            bluestar_config.cam1ip,
                            bluestar_config.cam2ip,
                            bluestar_config.cam3ip,
                            bluestar_config.cam4ip,
                        };

                        for (size_t cam = 0; cam < 4; ++cam) {
                            ImGui::TableNextRow();

                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("Camera %zu", cam + 1);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::SetNextItemWidth(-FLT_MIN); // -FLT_MIN Sets it to be as wide as the col
                            ImGui::InputText(
                                (std::string("##camera_url_") + std::to_string(cam)).c_str(),
                                cameraUrls[cam],
                                512);

                            #if defined(BLUESTAR_CAMERA_BACKEND_WEBRTCBIN)
                                char* cameravcap[] = {
                                    bluestar_config.cam1_video_caps,
                                    bluestar_config.cam2_video_caps,
                                    bluestar_config.cam3_video_caps,
                                    bluestar_config.cam4_video_caps,
                                };

                                char* cameraacap[] = {
                                    bluestar_config.cam1_audio_caps,
                                    bluestar_config.cam2_audio_caps,
                                    bluestar_config.cam3_audio_caps,
                                    bluestar_config.cam4_audio_caps,
                                };

                                ImGui::TableSetColumnIndex(2);
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                ImGui::InputText(
                                    (std::string("##camera_video_caps_") + std::to_string(cam)).c_str(),
                                    cameravcap[cam],
                                    1024);

                                ImGui::TableSetColumnIndex(3);
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                ImGui::InputText(
                                    (std::string("##camera_audio_caps_") + std::to_string(cam)).c_str(),
                                    cameraacap[cam],
                                    1024);
                            #endif
                        }

                        ImGui::EndTable();
                    }

                    ImGui::SeparatorText("Screenshot Crop");
                    if (ImGui::BeginTable("Camera Crop", 6, ImGuiTableFlags_Borders |
                                     ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_SizingStretchSame |
                                     ImGuiTableFlags_Borders |
                                     ImGuiTableFlags_Resizable)) {
                    
                        ImGui::TableSetupColumn("Camera", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                        ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                        ImGui::TableSetupColumn("Crop Left", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Crop Right", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Crop Top", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Crop Bottom", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();
                        
                        bool* CropEnabled[] = {&cam1ScreenshotCropEnabled, &cam2ScreenshotCropEnabled, &cam3ScreenshotCropEnabled, &cam4ScreenshotCropEnabled,};
                        float* CropLeft[] = {&cam1ScreenshotCropLeft, &cam2ScreenshotCropLeft, &cam3ScreenshotCropLeft, &cam4ScreenshotCropLeft,};
                        float* CropRight[] = {&cam1ScreenshotCropRight, &cam2ScreenshotCropRight, &cam3ScreenshotCropRight, &cam4ScreenshotCropRight,};
                        float* CropTop[] = {&cam1ScreenshotCropTop, &cam2ScreenshotCropTop, &cam3ScreenshotCropTop, &cam4ScreenshotCropTop,};
                        float* CropBottom[] = {&cam1ScreenshotCropBottom, &cam2ScreenshotCropBottom, &cam3ScreenshotCropBottom, &cam4ScreenshotCropBottom,};
                        bool cropChanged[4] = {};

                        for (size_t cam = 0; cam < 4; ++cam) {
                            ImGui::TableNextRow();

                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("Camera %zu", cam + 1);

                            ImGui::TableSetColumnIndex(1);
                            cropChanged[cam] |= ImGui::Checkbox((std::string("##camera_crop_enabled_") + std::to_string(cam)).c_str(), CropEnabled[cam]);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            cropChanged[cam] |= ImGui::SliderFloat((std::string("##camera_crop_left_") + std::to_string(cam)).c_str(), CropLeft[cam], 0.0f, 0.95f,"%.2f");
                            ImGui::TableSetColumnIndex(3);
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            cropChanged[cam] |= ImGui::SliderFloat((std::string("##camera_crop_right_") + std::to_string(cam)).c_str(), CropRight[cam], 0.0f, 0.95f,"%.2f");
                            ImGui::TableSetColumnIndex(4);
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            cropChanged[cam] |= ImGui::SliderFloat((std::string("##camera_crop_top_") + std::to_string(cam)).c_str(), CropTop[cam], 0.0f, 0.95f,"%.2f");
                            ImGui::TableSetColumnIndex(5);
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            cropChanged[cam] |= ImGui::SliderFloat((std::string("##camera_crop_bottom_") + std::to_string(cam)).c_str(), CropBottom[cam], 0.0f, 0.95f,"%.2f");

                            if (cropChanged[cam]) {
                                normalizeScreenshotCrop(*CropLeft[cam], *CropRight[cam], *CropTop[cam], *CropBottom[cam]);
                                updateScreenshotCrop();
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();

                }

                if (ImGui::BeginTabItem("Controls")) {
                    ImGui::SeparatorText("Flip Cameras Vertically");
                    ImGui::Text("Q - Flip Camera 1 Vertically");
                    ImGui::Text("W - Flip Camera 2 Vertically");
                    ImGui::Text("E - Flip Camera 3 Vertically");
                    ImGui::Text("R - Flip Camera 4 Vertically");
                    ImGui::SeparatorText("Flip Cameras Horizontally");
                    ImGui::Text("Z - Flip Camera 1 Horizontally");
                    ImGui::Text("X - Flip Camera 2 Horizontally");
                    ImGui::Text("C - Flip Camera 3 Horizontally");
                    ImGui::Text("V - Flip Camera 4 Horizontally");
                    ImGui::SeparatorText("Screenshot Cameras");
                    ImGui::Text("A - Screenshot Camera 1");
                    ImGui::Text("S - Screenshot Camera 2");
                    ImGui::Text("D - Screenshot Camera 3");
                    ImGui::Text("F - Screenshot Camera 4");
                    ImGui::SeparatorText("Screenshot Crop");
                    ImGui::Text("1 - Crop Camera 1");
                    ImGui::Text("2 - Crop Camera 2");
                    ImGui::Text("3 - Crop Camera 3");
                    ImGui::Text("4 - Crop Camera 4");
                    ImGui::SeparatorText("Other");
                    ImGui::Text("T - Increment Section");
                    ImGui::Text("U - Upload to WebODM");
                    
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("About GUI")) {
                    ImGui::Text("Eastern Edge");
                    ImGui::Spacing();
                    ImGui::Text("BlueStar Science Officer GUI");
                    ImGui::Text("MATE ROV 2026");

                    ImGui::Spacing();
                    ImGui::Spacing();

                    ImGui::Text("Camera Backend:");
                    ImGui::SameLine(); 
                    ImGui::TextUnformatted(cameraStreamBackendName());

                    ImGui::Text("OpenGL Version:");
                    ImGui::SameLine(); 
                    ImGui::TextUnformatted((version ? reinterpret_cast<const char*>(version) : "null"));

                    ImGui::Text("OpenGL Renderer:");
                    ImGui::SameLine(); 
                    ImGui::TextUnformatted((renderer ? reinterpret_cast<const char*>(renderer) : "null"));
                    
                    ImGui::EndTabItem();
                }
                
                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        //camera window
        if (showCameraWindow) {
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, (io.DisplaySize.y - ImGui::GetFrameHeight())));
            ImGui::SetNextWindowPos(ImVec2{0, ImGui::GetFrameHeight()});
            ImGui::Begin("Camera Window", &showCameraWindow, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
        
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 availPos = ImGui::GetContentRegionAvail();

            ImGui::SetCursorPos(ImVec2(windowPos.x+8, windowPos.y+8));
            cam1.render(ImVec2((availPos.x-24)/2, (availPos.y-24)/2));
            ImGui::SetCursorPos(ImVec2((availPos.x-24)/2+16, windowPos.y+8));
            cam2.render(ImVec2((availPos.x-24)/2, (availPos.y-24)/2));
            ImGui::SetCursorPos(ImVec2(windowPos.x+8, (availPos.y-24)/2+35));
            cam3.render(ImVec2((availPos.x-24)/2, (availPos.y-24)/2));
            ImGui::SetCursorPos(ImVec2((availPos.x-24)/2+16, (availPos.y-24)/2+35));
            cam4.render(ImVec2((availPos.x-24)/2, (availPos.y-24)/2));

            ImGui::End();
        }

        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    cam1.stop();
    cam2.stop();
    cam3.stop();
    cam4.stop();

    glfwTerminate();

    return 0;
}
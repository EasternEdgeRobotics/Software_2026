#include "Image.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <nlohmann/json.hpp>
#include "Config.hpp"
#include "ROS.hpp"
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

using json = nlohmann::json;

BlueStarConfig bluestar_config;

bool showConfigWindow = false;
bool showCameraWindow = true;

vector<string> names;
vector<string> configs;

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

const char* app_id = "EasternEdge.BlueStar.OfficerFrontend";

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

int main(int argc, char **argv) {
    //initialize glfw, imgui, and rclcpp (ros)    
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

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, true);
    GLFWwindow* window = glfwCreateWindow(800, 800, "Eastern Edge BlueStar Science Officer GUI", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    #ifndef __APPLE__
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    #endif

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
    ImGui_ImplOpenGL3_Init("#version 330");
    rclcpp::init(argc, argv);
    
    //create images
    unsigned int noSignal = loadEmbeddedTexture(nosignal_jpg, nosignal_jpg_len);

    //get configs from ros, then set the names and configs
    std::array<std::vector<std::string>, 2> configRes = getConfigs();
    names = configRes[0];
    configs = configRes[1];

    // Load the bluestar config
    for (size_t i = 0; i < names.size(); i++) {
        if (names[i] == "bluestar_config")
        {
            json configData = json::parse(configs[i]);
            if (!configData["cam1ip"].is_null()) std::snprintf(bluestar_config.cam1ip, sizeof(bluestar_config.cam1ip), "%s",configData["cam1ip"].get<std::string>().c_str());
            if (!configData["cam2ip"].is_null()) std::snprintf(bluestar_config.cam2ip, sizeof(bluestar_config.cam2ip), "%s",configData["cam2ip"].get<std::string>().c_str());
            if (!configData["cam3ip"].is_null()) std::snprintf(bluestar_config.cam3ip, sizeof(bluestar_config.cam3ip), "%s",configData["cam3ip"].get<std::string>().c_str());
            if (!configData["cam4ip"].is_null()) std::snprintf(bluestar_config.cam4ip, sizeof(bluestar_config.cam4ip), "%s",configData["cam4ip"].get<std::string>().c_str());

            if (!configData["cam1_video_caps"].is_null()) std::snprintf(bluestar_config.cam1_video_caps, sizeof(bluestar_config.cam1_video_caps), "%s",configData["cam1_video_caps"].get<std::string>().c_str());
            if (!configData["cam1_audio_caps"].is_null()) std::snprintf(bluestar_config.cam1_audio_caps, sizeof(bluestar_config.cam1_audio_caps), "%s",configData["cam1_audio_caps"].get<std::string>().c_str());

            if (!configData["cam2_video_caps"].is_null()) std::snprintf(bluestar_config.cam2_video_caps, sizeof(bluestar_config.cam2_video_caps), "%s",configData["cam2_video_caps"].get<std::string>().c_str());
            if (!configData["cam2_audio_caps"].is_null()) std::snprintf(bluestar_config.cam2_audio_caps, sizeof(bluestar_config.cam2_audio_caps), "%s",configData["cam2_audio_caps"].get<std::string>().c_str());

            if (!configData["cam3_video_caps"].is_null()) std::snprintf(bluestar_config.cam3_video_caps, sizeof(bluestar_config.cam3_video_caps), "%s",configData["cam3_video_caps"].get<std::string>().c_str());
            if (!configData["cam3_audio_caps"].is_null()) std::snprintf(bluestar_config.cam3_audio_caps, sizeof(bluestar_config.cam3_audio_caps), "%s",configData["cam3_audio_caps"].get<std::string>().c_str());

            if (!configData["cam4_video_caps"].is_null()) std::snprintf(bluestar_config.cam4_video_caps, sizeof(bluestar_config.cam4_video_caps), "%s",configData["cam4_video_caps"].get<std::string>().c_str());
            if (!configData["cam4_audio_caps"].is_null()) std::snprintf(bluestar_config.cam4_audio_caps, sizeof(bluestar_config.cam4_audio_caps), "%s",configData["cam4_audio_caps"].get<std::string>().c_str());
            break;
        }
    }

    Camera cam1(bluestar_config.cam1ip, bluestar_config.cam1_video_caps, bluestar_config.cam1_audio_caps, noSignal, 1);
    Camera cam2(bluestar_config.cam2ip, bluestar_config.cam2_video_caps, bluestar_config.cam2_audio_caps, noSignal, 2);
    Camera cam3(bluestar_config.cam3ip, bluestar_config.cam3_video_caps, bluestar_config.cam3_audio_caps, noSignal, 3);
    Camera cam4(bluestar_config.cam4ip, bluestar_config.cam4_video_caps, bluestar_config.cam4_audio_caps, noSignal, 4);

    auto updateScreenshotSuffix = [&]() {
        const std::string suffix =
            "section_" + std::to_string(camScreenshotSection);

        cam1.setScreenshotSuffix(suffix);
        cam2.setScreenshotSuffix(suffix);
        cam3.setScreenshotSuffix(suffix);
        cam4.setScreenshotSuffix(suffix);

        std::cout << "Screenshot suffix set to: " << suffix << std::endl;
    };

    updateScreenshotSuffix();

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

        //top menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Cameras")) {
                if (ImGui::MenuItem("Open Camera Window")) {
                    showCameraWindow = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Info")) {
                if (ImGui::MenuItem("Open Info Window")) {
                    showConfigWindow = true;
                }           
                ImGui::EndMenu();
            }

            ImGui::SameLine();
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
            if (ImGui::Button("Increment Section")) {                
                camIncrementSectionPressed = true;
            }
            
            //fps counter
            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
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
            if (!cam1ScreenshotButtonPressedLatch) if (!cam1.screenshot()) RCLCPP_ERROR(rclcpp::get_logger("main"), "Failed to take screenshot for Cam1");;
            cam1ScreenshotButtonPressedLatch = true;
        } else {
            cam1ScreenshotButtonPressedLatch = false;
        }

        if (cam2ScreenshotButtonPressed) {
            if (!cam2ScreenshotButtonPressedLatch) if (!cam2.screenshot()) RCLCPP_ERROR(rclcpp::get_logger("main"), "Failed to take screenshot for Cam2");;
            cam2ScreenshotButtonPressedLatch = true;
        } else {
            cam2ScreenshotButtonPressedLatch = false;
        }

        if (cam3ScreenshotButtonPressed) {
            if (!cam3ScreenshotButtonPressedLatch) if (!cam3.screenshot()) RCLCPP_ERROR(rclcpp::get_logger("main"), "Failed to take screenshot for Cam3");;
            cam3ScreenshotButtonPressedLatch = true;
        } else {
            cam3ScreenshotButtonPressedLatch = false;
        }

        if (cam4ScreenshotButtonPressed) {
            if (!cam4ScreenshotButtonPressedLatch) if (!cam4.screenshot()) RCLCPP_ERROR(rclcpp::get_logger("main"), "Failed to take screenshot for Cam4");;
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
                updateScreenshotSuffix();
            }
            camIncrementSectionPressedLatch = true;
        } else {
            camIncrementSectionPressedLatch = false;
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

    rclcpp::shutdown();
    return 0;
}
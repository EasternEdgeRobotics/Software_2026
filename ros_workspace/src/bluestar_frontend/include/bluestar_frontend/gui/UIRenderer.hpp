#ifndef BLUESTAR_FRONTEND_GUI_UI_RENDERER_HPP
#define BLUESTAR_FRONTEND_GUI_UI_RENDERER_HPP

#include <string>
#include <vector>
#include <functional>
#include <utility>

#include <GLFW/glfw3.h>

#include "bluestar_frontend/core/RovState.hpp"
#include "bluestar_frontend/core/ConfigLoader.hpp"

namespace bluestar {

/**
 * Callback type for saving configurations.
 * Parameters: (config_name, json_data)
 */
using SaveConfigCallback = std::function<void(const std::string&, const std::string&)>;

/**
 * Renders the ImGui user interface.
 * 
 * This class is part of the GUI layer (not reusable by autonomy nodes).
 * It reads from RovState and configs, and can trigger config saves via callbacks.
 */
class UIRenderer {
public:
    /**
     * Construct UI renderer.
     * 
     * @param save_callback Callback to invoke when saving a config
     */
    explicit UIRenderer(SaveConfigCallback save_callback);

    /**
     * Set the list of available configs (for Load menu).
     * 
     * @param configs Vector of (name, json_data) pairs
     */
    void setAvailableConfigs(std::vector<std::pair<std::string, std::string>> configs);

    /**
     * Render all UI elements for one frame.
     * Call between ImGui::NewFrame() and ImGui::Render().
     * 
     * @param window GLFW window handle
     * @param state ROV state (may be modified by UI interactions)
     * @param user_config User config (may be modified by UI interactions)
     * @param bluestar_config Bluestar config (may be modified by UI interactions)
     */
    void render(
        GLFWwindow* window,
        RovState& state,
        UserConfig& user_config,
        BluestarConfig& bluestar_config
    );

private:
    // Render the main menu bar
    void renderMenuBar(RovState& state, UserConfig& user_config);

    // Render the Config Editor window
    void renderConfigWindow(
        GLFWwindow* window,
        RovState& state,
        UserConfig& user_config,
        BluestarConfig& bluestar_config
    );

    // Render the Bluestar Config tab
    void renderBluestarConfigTab(
        GLFWwindow* window,
        RovState& state,
        BluestarConfig& bluestar_config
    );

    // Render the Controls (User) tab
    void renderControlsTab(GLFWwindow* window, UserConfig& user_config, const RovState& state);

    // Render the Save User Config tab
    void renderSaveUserConfigTab(GLFWwindow* window, UserConfig& user_config);

    // Render the Co-Pilot window
    void renderPilotWindow(RovState& state);

    SaveConfigCallback save_callback_;
    std::vector<std::pair<std::string, std::string>> available_configs_;

    // Window visibility state
    bool show_config_window_ = false;
    bool show_pilot_window_ = false;

    // Temporary state for config editor
    bool config_mode_checkbox_ = false;
    int selected_thruster_ = 0;

};

} // namespace bluestar

#endif // BLUESTAR_FRONTEND_GUI_UI_RENDERER_HPP

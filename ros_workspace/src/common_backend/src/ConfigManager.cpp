#include "rclcpp/rclcpp.hpp"
#include "eer_interfaces/srv/list_config.hpp"
#include "eer_interfaces/srv/get_config.hpp"
#include "eer_interfaces/srv/delete_config.hpp"
#include "eer_interfaces/msg/save_config.hpp"
#include <filesystem>
#include <fstream>

using namespace std;

using placeholders::_1;
using placeholders::_2;
using recursive_directory_iterator = filesystem::recursive_directory_iterator;

class ListConfigService : public rclcpp::Node {
    public:
        ListConfigService() : Node("list_config_service") {
            service_ = this->create_service<eer_interfaces::srv::ListConfig>("list_configs", bind(&ListConfigService::handle_request, this, _1, _2));
        }

    private:
        void handle_request(const shared_ptr<eer_interfaces::srv::ListConfig::Request> request, shared_ptr<eer_interfaces::srv::ListConfig::Response> response) {
            for (auto& configFile : recursive_directory_iterator("configs")) {
                string name = configFile.path().filename().string();

                if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
                    response->names.push_back(name.substr(0, name.size() - 5));
                }

                string path = configFile.path().string();
                ifstream file(path);
                if (file.is_open()) {
                    stringstream buffer;
                    buffer << file.rdbuf();
                    string content = buffer.str();
                    response->configs.push_back(content);
                }
            }
        }

        rclcpp::Service<eer_interfaces::srv::ListConfig>::SharedPtr service_;
};

class GetConfigService : public rclcpp::Node {
    public:
        GetConfigService() : Node("get_config_service") {
            service_ = this->create_service<eer_interfaces::srv::GetConfig>("get_config", bind(&GetConfigService::handle_request, this, _1, _2));
        }
    
    private:
        void handle_request(const shared_ptr<eer_interfaces::srv::GetConfig::Request> request, shared_ptr<eer_interfaces::srv::GetConfig::Response> response) {
            string path = "configs/" + request->name + ".json";

            if (!filesystem::exists(path)) response->config = "";
            else {
                ifstream file(path);

                if (!file.is_open()) {
                    throw ios_base::failure("Failed to open the file: " + path);
                }

                std::ostringstream buffer;
                buffer << file.rdbuf();
                string content = buffer.str();

                content.erase(remove(content.begin(), content.end(), '\n'), content.end());

                response->config = content;
            }
        }

        rclcpp::Service<eer_interfaces::srv::GetConfig>::SharedPtr service_;
};

class DeleteConfigService : public rclcpp::Node {
    public:
        DeleteConfigService() : Node("delete_config_service") {
            service_ = this->create_service<eer_interfaces::srv::DeleteConfig>("delete_config", bind(&DeleteConfigService::handle_request, this, _1, _2));
        }
    
    private:
        void handle_request(const shared_ptr<eer_interfaces::srv::DeleteConfig::Request> request, shared_ptr<eer_interfaces::srv::DeleteConfig::Response> response) {
            string path = "configs/" + request->name + ".json";

            try {
                if (std::filesystem::remove(path)) {
                    response->success = true;
                } else {
                    response->success = false;
                }
            } catch (const std::filesystem::filesystem_error& e) {
                response->success = false;
            }
        }

        rclcpp::Service<eer_interfaces::srv::DeleteConfig>::SharedPtr service_;
};

class SaveConfigSubscriber : public rclcpp::Node {
    public:
        SaveConfigSubscriber() : Node("save_config_subscriber") {
            subscription_ = this->create_subscription<eer_interfaces::msg::SaveConfig>("save_config", 1, std::bind(&SaveConfigSubscriber::topic_callback, this, _1));
        }
    private:
        void topic_callback(const eer_interfaces::msg::SaveConfig::SharedPtr msg) const {
            string path = "configs/" + msg->name + ".json";
            
            std::ofstream ofs;
            ofs.open(path, std::ios::out | std::ios::trunc);
            ofs << msg->data;
            ofs.close();
        }

        rclcpp::Subscription<eer_interfaces::msg::SaveConfig>::SharedPtr subscription_;
};

int main(int argc, char *argv[])
{
    filesystem::create_directory("configs");

    rclcpp::init(argc, argv);

    rclcpp::Node::SharedPtr listNode = make_shared<ListConfigService>();
    rclcpp::Node::SharedPtr getNode = make_shared<GetConfigService>();
    rclcpp::Node::SharedPtr delNode = make_shared<DeleteConfigService>();
    rclcpp::Node::SharedPtr saveNode = make_shared<SaveConfigSubscriber>();

    rclcpp::executors::StaticSingleThreadedExecutor executor;
    
    executor.add_node(listNode);
    executor.add_node(getNode);
    executor.add_node(delNode);
    executor.add_node(saveNode);

    executor.spin();
    rclcpp::shutdown();
    return 0;
}
/**
 * @brief System Monitor ROS 2 node
 * @file system_monitor_node.cpp
 * @addtogroup nodes
 * @author Nuno Marques <nuno.marques@dronesolutions.io>
 * @author Tanja Baumann <tanja@auterion.com
 * @date Jan 15, 2020
 *
 *      launch example:
 *      ros2 launch system_monitor_ros system_monitor.launch.py
 */

#include <system_monitor_ros/system_monitor.h>

#include <chrono>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

using namespace std::chrono_literals;

class OnboardComputerStatusPublisher : public rclcpp::Node {
 public:
  std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
      if (fgets(buffer.data(), 128, pipe.get()) != nullptr) result += buffer.data();
    }
    return result;
  }

  OnboardComputerStatusPublisher()
      : Node("system_monitor_node"),
        n_processes_(this->declare_parameter("n_processes", 8)),
        rate_(std::chrono::duration<double>(1 / this->declare_parameter("rate", 2.0))),
        enable_internal_pubs_(this->declare_parameter("enable_internal_pubs", false))

  {
    if (enable_internal_pubs_) {
      cpu_publisher_ = this->create_publisher<std_msgs::msg::String>("system_monitor/cpu_usage", 1);
      memory_pub_ = this->create_publisher<std_msgs::msg::String>("system_monitor/memory_usage", 1);
      processes_pub_ = this->create_publisher<std_msgs::msg::String>("system_monitor/processes", 1);
    }

    auto timer_callback = [this]() -> void {
      // Get CPU and Memory usage
      std::string cpu = exec(" top -bn 1 | sed -n 3p");
      std::string mem = exec(" top -bn 1 | sed -n 4p");

      // Check if the number of processes parameter was updated
      this->get_parameter("n_processes", n_processes_);

      RCLCPP_DEBUG(this->get_logger(), "n_processes: %d; rate: %f", n_processes_, rate_);

      // Get n_processes most CPU-hungry processes
      std::string command = "ps -e -o pid,pcpu,pmem,args --sort=-pcpu |  head -n ";
      command.append(std::to_string(n_processes_ + 1));
      command.append(" | cut -d' ' -f1-8");
      std::string processes = exec(command.c_str());

      auto cpu_msg = std_msgs::msg::String();
      cpu_msg.data = cpu;

      auto mem_msg = std_msgs::msg::String();
      mem_msg.data = mem;

      auto proc_msg = std_msgs::msg::String();
      proc_msg.data = processes;

      // Publish internal status data
      cpu_publisher_->publish(cpu_msg);
      memory_pub_->publish(mem_msg);
      processes_pub_->publish(proc_msg);
    };
    system_monitor_ = std::make_shared<SystemMonitor>();
    timer_ = this->create_wall_timer(rate_, timer_callback);
  }

 private:
  // SystemMonitor object ptr
  std::shared_ptr<SystemMonitor> system_monitor_;

  // Parameters
  int n_processes_;
  std::chrono::duration<double> rate_;
  bool enable_internal_pubs_;

  // ROS timer and publishers
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr cpu_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr memory_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr processes_pub_;
};

int main(int argc, char* argv[]) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<OnboardComputerStatusPublisher>());

  rclcpp::shutdown();
  return 0;
}

#include <algorithm>
#include <chrono>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include <gz/math/Pose3.hh>
#include <gz/math/Vector3.hh>
#include <gz/plugin/Register.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/Link.hh>
#include <gz/sim/Model.hh>
#include <gz/sim/System.hh>
#include <gz/sim/Util.hh>
#include <gz/sim/components/Pose.hh>

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <tello_msgs/msg/flight_data.hpp>
#include <tello_msgs/msg/tello_response.hpp>
#include <tello_msgs/srv/tello_action.hpp>

#include "pid.hpp"

namespace tello_gazebo
{
using namespace std::chrono_literals;

constexpr double MAX_XY_V = 8.0;
constexpr double MAX_Z_V = 4.0;
constexpr double MAX_ANG_V = M_PI;

constexpr double MAX_XY_A = 8.0;
constexpr double MAX_Z_A = 4.0;
constexpr double MAX_ANG_A = M_PI;

constexpr double TAKEOFF_Z = 1.0;
constexpr double TAKEOFF_Z_V = 0.5;

constexpr double LAND_Z = 0.1;
constexpr double LAND_Z_V = -0.5;

constexpr int BATTERY_DURATION = 6000;

inline double clamp(const double value, const double max)
{
  return std::max(-max, std::min(value, max));
}

class TelloSystem:
  public gz::sim::System,
  public gz::sim::ISystemConfigure,
  public gz::sim::ISystemPreUpdate
{
  enum class FlightState
  {
    landed,
    taking_off,
    flying,
    landing,
    dead_battery,
  };

public:
  void Configure(
    const gz::sim::Entity &_entity,
    const std::shared_ptr<const sdf::Element> &_sdf,
    gz::sim::EntityComponentManager &_ecm,
    gz::sim::EventManager &) override
  {
    this->model_ = gz::sim::Model(_entity);
    if (!this->model_.Valid(_ecm)) {
      gzerr << "TelloSystem must be attached to a model." << std::endl;
      return;
    }

    if (_sdf && _sdf->HasElement("link_name")) {
      this->link_name_ = _sdf->Get<std::string>("link_name");
    }
    if (_sdf && _sdf->HasElement("center_of_mass")) {
      this->center_of_mass_ = _sdf->Get<gz::math::Vector3d>("center_of_mass");
    }
    if (_sdf && _sdf->HasElement("battery_duration")) {
      this->battery_duration_ = _sdf->Get<int>("battery_duration");
    }
    if (_sdf && _sdf->HasElement("namespace")) {
      this->namespace_ = _sdf->Get<std::string>("namespace");
    }

    if (!rclcpp::ok()) {
      int argc = 0;
      char **argv = nullptr;
      rclcpp::init(argc, argv);
    }

    std::string node_ns = this->namespace_;
    if (!node_ns.empty() && node_ns.front() != '/') {
      node_ns = '/' + node_ns;
    }

    this->node_ = std::make_shared<rclcpp::Node>("tello_system", node_ns);

    this->flight_data_pub_ = this->node_->create_publisher<tello_msgs::msg::FlightData>("flight_data", 10);
    this->tello_response_pub_ = this->node_->create_publisher<tello_msgs::msg::TelloResponse>("tello_response", 10);

    this->command_srv_ = this->node_->create_service<tello_msgs::srv::TelloAction>(
      "tello_action",
      std::bind(
        &TelloSystem::command_callback,
        this,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    this->cmd_vel_sub_ = this->node_->create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel",
      10,
      std::bind(&TelloSystem::cmd_vel_callback, this, std::placeholders::_1));

    this->resolve_base_link(_ecm);
    this->transition(FlightState::landed);

    this->log_info();
  }

  void PreUpdate(const gz::sim::UpdateInfo &_info, gz::sim::EntityComponentManager &_ecm) override
  {
    if (_info.paused || !this->node_) {
      return;
    }

    rclcpp::spin_some(this->node_);

    if (!this->resolve_base_link(_ecm)) {
      return;
    }

    if (this->flight_state_ == FlightState::dead_battery) {
      return;
    }

    const auto sim_time = std::chrono::duration_cast<std::chrono::nanoseconds>(_info.simTime).count();
    if ((sim_time - this->last_10hz_time_ns_) > 100000000LL) {
      this->spin_10hz(sim_time, _ecm);
      this->last_10hz_time_ns_ = sim_time;
    }

    const double dt = this->last_update_time_ns_ > 0
      ? static_cast<double>(sim_time - this->last_update_time_ns_) * 1e-9
      : 0.001;
    this->last_update_time_ns_ = sim_time;

    if (this->flight_state_ == FlightState::landed) {
      return;
    }

    const auto linear_velocity_opt = this->base_link_.WorldLinearVelocity(_ecm);
    const auto angular_velocity_opt = this->base_link_.WorldAngularVelocity(_ecm);

    const gz::math::Vector3d linear_velocity = linear_velocity_opt.value_or(gz::math::Vector3d::Zero);
    const gz::math::Vector3d angular_velocity = angular_velocity_opt.value_or(gz::math::Vector3d::Zero);

    gz::math::Vector3d lin_ubar;
    gz::math::Vector3d ang_ubar;

    lin_ubar.X(this->x_controller_.calc(linear_velocity.X(), dt, 0.0));
    lin_ubar.Y(this->y_controller_.calc(linear_velocity.Y(), dt, 0.0));
    lin_ubar.Z(this->z_controller_.calc(linear_velocity.Z(), dt, 0.0));
    ang_ubar.Z(this->yaw_controller_.calc(angular_velocity.Z(), dt, 0.0));

    lin_ubar.X(clamp(lin_ubar.X(), MAX_XY_A));
    lin_ubar.Y(clamp(lin_ubar.Y(), MAX_XY_A));
    lin_ubar.Z(clamp(lin_ubar.Z(), MAX_Z_A));
    ang_ubar.Z(clamp(ang_ubar.Z(), MAX_ANG_A));

    lin_ubar -= this->gravity_;

    const double mass = 0.1;
    const gz::math::Vector3d moi{0.000290833, 0.00054, 0.000290833};

    const gz::math::Vector3d force = lin_ubar * mass;
    const gz::math::Vector3d torque(
      ang_ubar.X() * moi.X(),
      ang_ubar.Y() * moi.Y(),
      ang_ubar.Z() * moi.Z());

    this->base_link_.AddWorldWrench(_ecm, force, torque);
  }

private:
  bool resolve_base_link(gz::sim::EntityComponentManager &_ecm)
  {
    if (this->base_link_entity_ != gz::sim::kNullEntity) {
      return true;
    }

    auto entity = this->model_.LinkByName(_ecm, this->link_name_);
    if (entity == gz::sim::kNullEntity) {
      entity = this->model_.CanonicalLink(_ecm);
    }

    if (entity == gz::sim::kNullEntity) {
      return false;
    }

    this->base_link_entity_ = entity;
    this->base_link_ = gz::sim::Link(entity);
    this->base_link_.EnableVelocityChecks(_ecm, true);
    return true;
  }

  void set_target_velocities(double x, double y, double z, double yaw)
  {
    this->x_controller_.set_target(x);
    this->y_controller_.set_target(y);
    this->z_controller_.set_target(z);
    this->yaw_controller_.set_target(yaw);
  }

  void set_target_velocities(const std::string &rc_command)
  {
    double x;
    double y;
    double z;
    double yaw;

    try {
      std::istringstream iss(rc_command);
      std::string token;
      iss >> token;
      iss >> x;
      iss >> y;
      iss >> z;
      iss >> yaw;
    } catch (const std::exception &e) {
      RCLCPP_ERROR(this->node_->get_logger(), "can't parse rc command '%s': %s", rc_command.c_str(), e.what());
      return;
    }

    this->set_target_velocities(
      x * MAX_XY_V,
      y * MAX_XY_V,
      z * MAX_Z_V,
      yaw * MAX_ANG_V);
  }

  void transition(FlightState next)
  {
    if (this->node_) {
      RCLCPP_INFO(
        this->node_->get_logger(),
        "transition from '%s' to '%s'",
        this->state_strs_[this->flight_state_],
        this->state_strs_[next]);
    }

    this->flight_state_ = next;

    switch (this->flight_state_) {
      case FlightState::landed:
      case FlightState::flying:
      case FlightState::dead_battery:
        this->set_target_velocities(0, 0, 0, 0);
        break;
      case FlightState::taking_off:
        this->set_target_velocities(0, 0, TAKEOFF_Z_V, 0);
        break;
      case FlightState::landing:
        this->set_target_velocities(0, 0, LAND_Z_V, 0);
        break;
    }
  }

  bool is_prefix(const std::string &prefix, const std::string &value)
  {
    return value.rfind(prefix, 0) == 0;
  }

  void command_callback(
    const std::shared_ptr<rmw_request_id_t>,
    const std::shared_ptr<tello_msgs::srv::TelloAction::Request> request,
    std::shared_ptr<tello_msgs::srv::TelloAction::Response> response)
  {
    if (request->cmd == "takeoff" && this->flight_state_ == FlightState::landed) {
      this->transition(FlightState::taking_off);
      response->rc = response->OK;
    } else if (request->cmd == "land" && this->flight_state_ == FlightState::flying) {
      this->transition(FlightState::landing);
      response->rc = response->OK;
    } else if (this->is_prefix("rc", request->cmd) && this->flight_state_ == FlightState::flying) {
      this->set_target_velocities(request->cmd);
      response->rc = response->OK;
    } else {
      response->rc = response->ERROR_BUSY;
    }
  }

  void cmd_vel_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
  {
    if (this->flight_state_ == FlightState::flying) {
      this->set_target_velocities(
        msg->linear.x * MAX_XY_V,
        msg->linear.y * MAX_XY_V,
        msg->linear.z * MAX_Z_V,
        msg->angular.z * MAX_ANG_V);
    }
  }

  void respond_ok()
  {
    tello_msgs::msg::TelloResponse msg;
    msg.rc = msg.OK;
    msg.str = "ok";
    this->tello_response_pub_->publish(msg);
  }

  void spin_10hz(int64_t sim_time_ns, gz::sim::EntityComponentManager &_ecm)
  {
    const double sim_seconds = static_cast<double>(sim_time_ns) * 1e-9;

    int battery_percent = static_cast<int>((this->battery_duration_ - sim_seconds) / this->battery_duration_ * 100.0);
    if (battery_percent <= 0) {
      this->transition(FlightState::dead_battery);
      return;
    }

    tello_msgs::msg::FlightData flight_data;
    flight_data.header.stamp = rclcpp::Time(sim_time_ns, RCL_ROS_TIME);
    flight_data.sdk = flight_data.SDK_1_3;
    flight_data.bat = battery_percent;
    this->flight_data_pub_->publish(flight_data);

    if (this->base_link_entity_ == gz::sim::kNullEntity) {
      return;
    }

    const auto pose_comp = _ecm.Component<gz::sim::components::WorldPose>(this->base_link_entity_);
    if (!pose_comp) {
      return;
    }

    const auto &pose = pose_comp->Data();
    if (this->flight_state_ == FlightState::taking_off && pose.Pos().Z() > TAKEOFF_Z) {
      this->transition(FlightState::flying);
      this->respond_ok();
    } else if (this->flight_state_ == FlightState::landing && pose.Pos().Z() < LAND_Z) {
      this->transition(FlightState::landed);
      this->respond_ok();
    }
  }

  void log_info()
  {
    std::cout << std::fixed << std::setprecision(2) << std::endl;
    std::cout << "TELLO GZ SYSTEM" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "link_name: " << this->link_name_ << std::endl;
    std::cout << "center_of_mass: " << this->center_of_mass_ << std::endl;
    std::cout << "gravity: " << this->gravity_ << std::endl;
    std::cout << "battery_duration: " << this->battery_duration_ << std::endl;
    std::cout << "namespace: " << this->namespace_ << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << std::endl;
  }

private:
  std::map<FlightState, const char *> state_strs_ {
    {FlightState::landed, "landed"},
    {FlightState::taking_off, "taking_off"},
    {FlightState::flying, "flying"},
    {FlightState::landing, "landing"},
    {FlightState::dead_battery, "dead_battery"},
  };

  FlightState flight_state_{FlightState::landed};

  gz::sim::Model model_{gz::sim::kNullEntity};
  gz::sim::Entity base_link_entity_{gz::sim::kNullEntity};
  gz::sim::Link base_link_{gz::sim::kNullEntity};

  std::string namespace_{"drone1"};
  std::string link_name_{"base_link"};

  gz::math::Vector3d gravity_{0.0, 0.0, -9.81};
  gz::math::Vector3d center_of_mass_{0.0, 0.0, 0.0};

  int battery_duration_{BATTERY_DURATION};
  int64_t last_update_time_ns_{0};
  int64_t last_10hz_time_ns_{0};

  rclcpp::Node::SharedPtr node_;

  rclcpp::Publisher<tello_msgs::msg::FlightData>::SharedPtr flight_data_pub_;
  rclcpp::Publisher<tello_msgs::msg::TelloResponse>::SharedPtr tello_response_pub_;

  rclcpp::Service<tello_msgs::srv::TelloAction>::SharedPtr command_srv_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;

  pid::Controller x_controller_{false, 2, 0, 0};
  pid::Controller y_controller_{false, 2, 0, 0};
  pid::Controller z_controller_{false, 2, 0, 0};
  pid::Controller yaw_controller_{false, 2, 0, 0};
};

}  // namespace tello_gazebo

GZ_ADD_PLUGIN(
  tello_gazebo::TelloSystem,
  ::gz::sim::System,
  tello_gazebo::TelloSystem::ISystemConfigure,
  tello_gazebo::TelloSystem::ISystemPreUpdate)

GZ_ADD_PLUGIN_ALIAS(tello_gazebo::TelloSystem, "tello_gazebo::TelloSystem")

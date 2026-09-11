#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/image_encodings.hpp"
#include "geometry_msgs/msg/transform_stamped.hpp"

#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <tf2_ros/transform_broadcaster.h>
#include <image_geometry/pinhole_camera_model.hpp>

#include <gz/msgs/pose.pb.h>
#include <gz/msgs/boolean.pb.h>
#include <gz/transport/Node.hh>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>

class GazeboTwin : public rclcpp::Node
{
public:
  GazeboTwin() : Node("gazebo_twin")
  {
    real_depth_sub_ = this->create_subscription<sensor_msgs::msg::Image>(
      "/camera/depth/image_raw", 3,
      std::bind(&GazeboTwin::real_depth_callback, this, std::placeholders::_1));

    real_camera_info_sub_ = this->create_subscription<sensor_msgs::msg::CameraInfo>(
      "/camera/depth/camera_info", 3,
      std::bind(&GazeboTwin::real_camera_info_callback, this, std::placeholders::_1));

    real_point_cloud_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
      "/camera/depth_registered/points", 3,
      std::bind(&GazeboTwin::real_point_cloud_callback, this, std::placeholders::_1));

    real_color_sub_ = this->create_subscription<sensor_msgs::msg::Image>(
      "/camera/color/image_raw", 3,
      std::bind(&GazeboTwin::real_color_callback, this, std::placeholders::_1));

    twin_depth_pub_ = this->create_publisher<sensor_msgs::msg::Image>(
      "/twin/depth/image_raw", 10);
    twin_color_pub_ = this->create_publisher<sensor_msgs::msg::Image>(
      "/twin/color/image_raw", 10);
    twin_point_cloud_pub_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(
      "/twin/depth_registered/points", 10);

    transform_broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);

    this->declare_parameter<std::string>("gz_setpose_service", "/world/default/set_pose");

    this->declare_parameter<std::string>("twin_camera_model", "twin_camera");
    this->declare_parameter<double>("twin_camera_x", 0.0);
    this->declare_parameter<double>("twin_camera_y", 0.0);
    this->declare_parameter<double>("twin_camera_z", 1.0);
    this->declare_parameter<double>("twin_camera_qx", 0.0);
    this->declare_parameter<double>("twin_camera_qy", 0.0);
    this->declare_parameter<double>("twin_camera_qz", 0.0);
    this->declare_parameter<double>("twin_camera_qw", 1.0);
    this->declare_parameter<bool>("enable_twin_sync", true);

    this->get_parameter("gz_setpose_service", gz_setpose_service_);
    this->get_parameter("twin_camera_model", twin_camera_model_);
    this->get_parameter("twin_camera_x", twin_camera_x_);
    this->get_parameter("twin_camera_y", twin_camera_y_);
    this->get_parameter("twin_camera_z", twin_camera_z_);
    this->get_parameter("twin_camera_qx", twin_camera_qx_);
    this->get_parameter("twin_camera_qy", twin_camera_qy_);
    this->get_parameter("twin_camera_qz", twin_camera_qz_);
    this->get_parameter("twin_camera_qw", twin_camera_qw_);
    this->get_parameter("enable_twin_sync", enable_twin_sync_);

    current_camera_x_ = twin_camera_x_;
    current_camera_y_ = twin_camera_y_;
    current_camera_z_ = twin_camera_z_;
    current_camera_qx_ = twin_camera_qx_;
    current_camera_qy_ = twin_camera_qy_;
    current_camera_qz_ = twin_camera_qz_;
    current_camera_qw_ = twin_camera_qw_;

    sync_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(100),
      std::bind(&GazeboTwin::sync_twin_pose, this));

    RCLCPP_INFO(this->get_logger(), "Gazebo Twin Node started");
    RCLCPP_INFO(this->get_logger(), "Twin camera model: %s", twin_camera_model_.c_str());
    RCLCPP_INFO(this->get_logger(), "Initial pose: (%.2f, %.2f, %.2f)",
      twin_camera_x_, twin_camera_y_, twin_camera_z_);
  }

private:
  void real_camera_info_callback(const sensor_msgs::msg::CameraInfo::ConstSharedPtr& msg)
  {
    camera_model_.fromCameraInfo(msg);
    last_camera_info_ = *msg;
    got_camera_info_ = true;
  }

  void generate_point_cloud_from_depth(const sensor_msgs::msg::Image::ConstSharedPtr& depth_msg)
  {
    if (!got_camera_info_) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
        "No camera info received yet, skipping point cloud generation");
      return;
    }

    sensor_msgs::msg::Image::SharedPtr color_msg_copy;
    {
      std::lock_guard<std::mutex> lock(color_mutex_);
      color_msg_copy = latest_color_image_;
    }

    const bool have_color = color_msg_copy != nullptr &&
      color_msg_copy->encoding == sensor_msgs::image_encodings::RGB8 &&
      color_msg_copy->width == depth_msg->width &&
      color_msg_copy->height == depth_msg->height;

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>());
    cloud->width = depth_msg->width;
    cloud->height = depth_msg->height;
    cloud->is_dense = false;
    cloud->points.resize(cloud->width * cloud->height);

    const float center_x = static_cast<float>(camera_model_.cx());
    const float center_y = static_cast<float>(camera_model_.cy());
    const float constant_x = static_cast<float>(1.0 / camera_model_.fx());
    const float constant_y = static_cast<float>(1.0 / camera_model_.fy());
    const float bad_point = std::numeric_limits<float>::quiet_NaN();

    size_t point_idx = 0;
    if (depth_msg->encoding == sensor_msgs::image_encodings::TYPE_16UC1) {
      const uint16_t* depth_row = reinterpret_cast<const uint16_t*>(&depth_msg->data[0]);
      const size_t row_step = depth_msg->step / sizeof(uint16_t);
      for (uint32_t v = 0; v < depth_msg->height; ++v, depth_row += row_step) {
        const uint8_t* color_row = have_color ?
          &color_msg_copy->data[static_cast<size_t>(v) * color_msg_copy->step] : nullptr;
        for (uint32_t u = 0; u < depth_msg->width; ++u, ++point_idx) {
          const uint16_t depth_mm = depth_row[u];
          auto& point = cloud->points[point_idx];
          if (depth_mm == 0) {
            point.x = bad_point;
            point.y = bad_point;
            point.z = bad_point;
            continue;
          }
          const float depth_m = depth_mm * 0.001f;
          point.x = (static_cast<float>(u) - center_x) * depth_m * constant_x;
          point.y = (static_cast<float>(v) - center_y) * depth_m * constant_y;
          point.z = depth_m;
          if (color_row) {
            const uint8_t* pixel = color_row + static_cast<size_t>(u) * 3;
            point.r = pixel[0];
            point.g = pixel[1];
            point.b = pixel[2];
          }
        }
      }
    } else if (depth_msg->encoding == sensor_msgs::image_encodings::TYPE_32FC1) {
      const float* depth_row = reinterpret_cast<const float*>(&depth_msg->data[0]);
      const size_t row_step = depth_msg->step / sizeof(float);
      for (uint32_t v = 0; v < depth_msg->height; ++v, depth_row += row_step) {
        const uint8_t* color_row = have_color ?
          &color_msg_copy->data[static_cast<size_t>(v) * color_msg_copy->step] : nullptr;
        for (uint32_t u = 0; u < depth_msg->width; ++u, ++point_idx) {
          const float depth = depth_row[u];
          auto& point = cloud->points[point_idx];
          if (!std::isfinite(depth) || depth <= 0.0f) {
            point.x = bad_point;
            point.y = bad_point;
            point.z = bad_point;
            continue;
          }
          point.x = (static_cast<float>(u) - center_x) * depth * constant_x;
          point.y = (static_cast<float>(v) - center_y) * depth * constant_y;
          point.z = depth;
          if (color_row) {
            const uint8_t* pixel = color_row + static_cast<size_t>(u) * 3;
            point.r = pixel[0];
            point.g = pixel[1];
            point.b = pixel[2];
          }
        }
      }
    } else {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
        "Unsupported depth encoding: %s", depth_msg->encoding.c_str());
      return;
    }

    sensor_msgs::msg::PointCloud2 cloud_msg;
    pcl::toROSMsg(*cloud, cloud_msg);
    cloud_msg.header = depth_msg->header;
    twin_point_cloud_pub_->publish(cloud_msg);

    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
      "Generated point cloud from depth image: %ux%u points",
      depth_msg->width, depth_msg->height);
  }

  void sync_twin_pose()
  {
    this->get_parameter("twin_camera_x", current_camera_x_);
    this->get_parameter("twin_camera_y", current_camera_y_);
    this->get_parameter("twin_camera_z", current_camera_z_);
    this->get_parameter("twin_camera_qx", current_camera_qx_);
    this->get_parameter("twin_camera_qy", current_camera_qy_);
    this->get_parameter("twin_camera_qz", current_camera_qz_);
    this->get_parameter("twin_camera_qw", current_camera_qw_);
    this->get_parameter("enable_twin_sync", enable_twin_sync_);

    auto now = this->get_clock()->now();

    geometry_msgs::msg::TransformStamped transform_stamped;
    transform_stamped.header.stamp = now;
    transform_stamped.header.frame_id = "world";
    transform_stamped.child_frame_id = "twin_camera";
    transform_stamped.transform.translation.x = current_camera_x_;
    transform_stamped.transform.translation.y = current_camera_y_;
    transform_stamped.transform.translation.z = current_camera_z_;
    transform_stamped.transform.rotation.x = current_camera_qx_;
    transform_stamped.transform.rotation.y = current_camera_qy_;
    transform_stamped.transform.rotation.z = current_camera_qz_;
    transform_stamped.transform.rotation.w = current_camera_qw_;

    transform_broadcaster_->sendTransform(transform_stamped);

    geometry_msgs::msg::TransformStamped camera_transform;
    camera_transform.header.stamp = now;
    camera_transform.header.frame_id = "twin_camera";
    camera_transform.child_frame_id = "camera_link";
    camera_transform.transform.rotation.w = 1.0;

    transform_broadcaster_->sendTransform(camera_transform);

    if (!enable_twin_sync_) {
      RCLCPP_DEBUG_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
        "Twin sync disabled, TF only");
      return;
    }

    gz::msgs::Pose pose;
    pose.set_name(twin_camera_model_);
    pose.mutable_position()->set_x(current_camera_x_);
    pose.mutable_position()->set_y(current_camera_y_);
    pose.mutable_position()->set_z(current_camera_z_);
    pose.mutable_orientation()->set_x(current_camera_qx_);
    pose.mutable_orientation()->set_y(current_camera_qy_);
    pose.mutable_orientation()->set_z(current_camera_qz_);
    pose.mutable_orientation()->set_w(current_camera_qw_);

    gz::msgs::Boolean rep;
    bool result = false;
    const bool executed = gz_node_.Request(gz_setpose_service_, pose, 1000, rep, result);
    if (!executed || !result || !rep.data()) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
        "Failed to set twin pose via %s for %s",
        gz_setpose_service_.c_str(), twin_camera_model_.c_str());
      return;
    }

    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
      "Syncing twin pose: (%.2f, %.2f, %.2f) quat (%.2f, %.2f, %.2f, %.2f)",
      current_camera_x_, current_camera_y_, current_camera_z_,
      current_camera_qx_, current_camera_qy_, current_camera_qz_, current_camera_qw_);
  }

  void real_depth_callback(const sensor_msgs::msg::Image::ConstSharedPtr& msg)
  {
    twin_depth_pub_->publish(*msg);

    auto now = std::chrono::steady_clock::now();
    if (!native_pc_available_ &&
        std::chrono::duration<double>(now - last_pc_gen_time_).count() > 0.33) {
      generate_point_cloud_from_depth(msg);
      last_pc_gen_time_ = now;
    }
  }

  void real_color_callback(const sensor_msgs::msg::Image::ConstSharedPtr& msg)
  {
    std::lock_guard<std::mutex> lock(color_mutex_);
    latest_color_image_ = std::make_shared<sensor_msgs::msg::Image>(*msg);
    twin_color_pub_->publish(*msg);
  }

  void real_point_cloud_callback(const sensor_msgs::msg::PointCloud2::ConstSharedPtr& msg)
  {
    native_pc_available_ = true;
    twin_point_cloud_pub_->publish(*msg);
  }

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr real_depth_sub_;
  rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr real_camera_info_sub_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr real_point_cloud_sub_;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr real_color_sub_;

  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr twin_depth_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr twin_color_pub_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr twin_point_cloud_pub_;

  std::shared_ptr<tf2_ros::TransformBroadcaster> transform_broadcaster_;
  gz::transport::Node gz_node_;

  rclcpp::TimerBase::SharedPtr sync_timer_;

  std::mutex color_mutex_;
  sensor_msgs::msg::Image::SharedPtr latest_color_image_;

  image_geometry::PinholeCameraModel camera_model_;
  bool got_camera_info_ = false;
  sensor_msgs::msg::CameraInfo last_camera_info_;

  std::string gz_setpose_service_;
  std::string twin_camera_model_;
  double twin_camera_x_;
  double twin_camera_y_;
  double twin_camera_z_;
  double twin_camera_qx_;
  double twin_camera_qy_;
  double twin_camera_qz_;
  double twin_camera_qw_;
  bool enable_twin_sync_;

  double current_camera_x_ = 0.0;
  double current_camera_y_ = 0.0;
  double current_camera_z_ = 1.0;
  double current_camera_qx_ = 0.0;
  double current_camera_qy_ = 0.0;
  double current_camera_qz_ = 0.0;
  double current_camera_qw_ = 1.0;

  bool native_pc_available_ = false;
  std::chrono::steady_clock::time_point last_pc_gen_time_{};
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<GazeboTwin>());
  rclcpp::shutdown();
  return 0;
}
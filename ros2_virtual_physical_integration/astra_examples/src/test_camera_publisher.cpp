#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/camera_info.hpp"

#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>

class TestCameraPublisher : public rclcpp::Node
{
public:
  TestCameraPublisher() : Node("test_camera_publisher")
  {
    depth_pub_ = this->create_publisher<sensor_msgs::msg::Image>(
      "/camera/depth/image_raw", 10);
    color_pub_ = this->create_publisher<sensor_msgs::msg::Image>(
      "/camera/color/image_raw", 10);
    point_cloud_pub_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(
      "/camera/depth_registered/points", 10);
    camera_info_pub_ = this->create_publisher<sensor_msgs::msg::CameraInfo>(
      "/camera/depth/camera_info", 10);

    timer_ = this->create_wall_timer(
      std::chrono::milliseconds(100),
      std::bind(&TestCameraPublisher::publish_data, this));

    RCLCPP_INFO(this->get_logger(), "Test Camera Publisher started");
  }

private:
  void publish_data()
  {
    publish_camera_info();
    publish_depth_image();
    publish_color_image();
    publish_point_cloud();
  }

  void publish_camera_info()
  {
    sensor_msgs::msg::CameraInfo info;
    info.header.stamp = this->get_clock()->now();
    info.header.frame_id = "camera_link";
    info.width = 640;
    info.height = 480;
    info.distortion_model = "plumb_bob";
    info.d = {0.0, 0.0, 0.0, 0.0, 0.0};
    info.k = {525.0, 0.0, 319.5, 0.0, 525.0, 239.5, 0.0, 0.0, 1.0};
    info.r = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    info.p = {525.0, 0.0, 319.5, 0.0, 0.0, 525.0, 239.5, 0.0, 0.0, 0.0, 1.0, 0.0};
    camera_info_pub_->publish(info);
  }

  void publish_depth_image()
  {
    sensor_msgs::msg::Image depth_msg;
    depth_msg.header.stamp = this->get_clock()->now();
    depth_msg.header.frame_id = "camera_link";
    depth_msg.width = 640;
    depth_msg.height = 480;
    depth_msg.encoding = "16UC1";
    depth_msg.step = 640 * 2;
    depth_msg.data.resize(640 * 480 * 2, 0);

    for (int y = 0; y < 480; y++) {
      for (int x = 0; x < 640; x++) {
        float depth = 1.0 + (x / 640.0) * 2.0;
        uint16_t depth_uint16 = static_cast<uint16_t>(depth * 1000);
        int idx = (y * 640 + x) * 2;
        depth_msg.data[idx] = depth_uint16 & 0xFF;
        depth_msg.data[idx + 1] = (depth_uint16 >> 8) & 0xFF;
      }
    }
    depth_pub_->publish(depth_msg);
  }

  void publish_color_image()
  {
    sensor_msgs::msg::Image color_msg;
    color_msg.header.stamp = this->get_clock()->now();
    color_msg.header.frame_id = "camera_link";
    color_msg.width = 640;
    color_msg.height = 480;
    color_msg.encoding = "rgb8";
    color_msg.step = 640 * 3;
    color_msg.data.resize(640 * 480 * 3, 0);

    for (int y = 0; y < 480; y++) {
      for (int x = 0; x < 640; x++) {
        int idx = (y * 640 + x) * 3;
        color_msg.data[idx] = static_cast<uint8_t>((x / 640.0) * 255);
        color_msg.data[idx + 1] = static_cast<uint8_t>((y / 480.0) * 255);
        color_msg.data[idx + 2] = 128;
      }
    }
    color_pub_->publish(color_msg);
  }

  void publish_point_cloud()
  {
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>());
    cloud->width = 100;
    cloud->height = 100;
    cloud->is_dense = false;
    cloud->points.resize(cloud->width * cloud->height);

    for (int y = 0; y < 100; y++) {
      for (int x = 0; x < 100; x++) {
        int idx = y * 100 + x;
        cloud->points[idx].x = (x - 50) * 0.02;
        cloud->points[idx].y = (y - 50) * 0.02;
        cloud->points[idx].z = 1.0 + (x / 100.0) * 2.0;
        cloud->points[idx].r = static_cast<uint8_t>((x / 100.0) * 255);
        cloud->points[idx].g = static_cast<uint8_t>((y / 100.0) * 255);
        cloud->points[idx].b = 128;
      }
    }

    sensor_msgs::msg::PointCloud2 cloud_msg;
    pcl::toROSMsg(*cloud, cloud_msg);
    cloud_msg.header.stamp = this->get_clock()->now();
    cloud_msg.header.frame_id = "camera_link";
    point_cloud_pub_->publish(cloud_msg);
  }

  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr depth_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr color_pub_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr point_cloud_pub_;
  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr camera_info_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TestCameraPublisher>());
  rclcpp::shutdown();
  return 0;
}
#pragma once

#include "attribute_system.hpp"

namespace fibril_transport_common
{

// ========================================
// Built-in Attribute Types
// ========================================

/**
 * @brief #[ros_type("geometry_msgs/msg/Twist")]
 * 
 * Maps a Fibril struct to a ROS message type.
 */
class RosTypeAttribute : public AttributeBase
{
public:
  std::string ros_message_type;

  explicit RosTypeAttribute(std::string type);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<RosTypeAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief #[ros_type("geometry_msgs/msg/Twist")]
 * 
 * Maps a Fibril struct to a ROS message type.
 */
class RosServiceAttribute : public AttributeBase
{
public:
  std::string ros_service_type;

  explicit RosServiceAttribute(std::string type);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<RosServiceAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief #[ros_map("linear.x")]
 * 
 * Maps a Fibril field to a ROS message field path.
 */
class RosMapAttribute : public AttributeBase
{
public:
  std::string field_path;

  explicit RosMapAttribute(std::string path);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<RosMapAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief #[ros("/cmd_vel")] or #[ros("~/voltage")]
 * 
 * Specifies the ROS topic name for a port.
 */
class RosAttribute : public AttributeBase
{
public:
  std::string topic_name;

  explicit RosAttribute(std::string name);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<RosAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief #[unit("m/s")]
 * 
 * Specifies the physical unit of a field (for documentation/visualization).
 */
class UnitAttribute : public AttributeBase
{
public:
  std::string unit;

  explicit UnitAttribute(std::string u);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<UnitAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief #[ros_frame_id("base_link")]
 * 
 * Specifies the ROS frame_id for coordinate frames.
 */
class RosFrameIdAttribute : public AttributeBase
{
public:
  std::string frame_id;

  explicit RosFrameIdAttribute(std::string id);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<RosFrameIdAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief Register all built-in attribute deserializers
 * 
 * This should be called once at program startup.
 */
void registerBuiltinAttributes();

}  // namespace fibril_transport_common

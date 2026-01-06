#pragma once

#include <fibril_transport_common/ros_attribute.hpp>

#include "parser/attribute_parser.hpp"

namespace fibril
{

// ========================================
// Built-in Attribute Types
// ========================================

/**
 * @brief #[ros_type(geometry_msgs/msg/Twist)]
 * 
 * Maps a Fibril struct to a ROS message type.
 */
class RosTypeAttributeParser : public AttributeParser
{
public:
  std::string name() const override { return "ros_type"; }

  std::unique_ptr<fibril_transport_common::AttributeBase> parse(AttributeParameter & param) override
  {
    if (param.values.size() != 1) {
      throw std::runtime_error("ros_type attribute must have exactly one value");
    }

    const auto & value = std::get<AttributeParameter::Identifier>(param.values[0]);

    return std::make_unique<fibril_transport_common::RosTypeAttribute>(value.value);
  }
};

/**
 * @brief #[ros_service(std_srvs/srv/Empty)]
 * 
 * Maps a Fibril service to a ROS service type.
 */
class RosServiceAttributeParser : public AttributeParser
{
public:
  std::string name() const override { return "ros_service"; }

  std::unique_ptr<fibril_transport_common::AttributeBase> parse(AttributeParameter & param) override
  {
    if (param.values.size() != 1) {
      throw std::runtime_error("ros_service attribute must have exactly one value");
    }

    const auto & value = std::get<AttributeParameter::Identifier>(param.values[0]);

    return std::make_unique<fibril_transport_common::RosServiceAttribute>(value.value);
  }
};

/**
 * @brief #[ros_map(linear.x)]
 * 
 * Maps a Fibril field to a ROS message field path.
 */
class RosMapAttributeParser : public AttributeParser
{
public:
  std::string name() const override { return "ros_map"; }

  std::unique_ptr<fibril_transport_common::AttributeBase> parse(AttributeParameter & param) override
  {
    if (param.values.size() != 1) {
      throw std::runtime_error("ros_map attribute must have exactly one value");
    }

    const auto & value = std::get<AttributeParameter::Identifier>(param.values[0]);

    return std::make_unique<fibril_transport_common::RosMapAttribute>(value.value);
  }
};

/**
 * @brief #[ros("/cmd_vel")] or #[ros("~/voltage")]
 * 
 * Specifies the ROS topic name for a port.
 */
class RosAttributeParser : public AttributeParser
{
public:
  std::string name() const override { return "ros"; }

  std::unique_ptr<fibril_transport_common::AttributeBase> parse(AttributeParameter & param) override
  {
    if (param.values.size() != 1) {
      throw std::runtime_error("ros attribute must have exactly one value");
    }

    const auto & value = std::get<AttributeParameter::String>(param.values[0]);

    return std::make_unique<fibril_transport_common::RosAttribute>(value.str);
  }
};

/**
 * @brief #[unit("m/s")]
 * 
 * Specifies the physical unit of a field (for documentation/visualization).
 */
class UnitAttributeParser : public AttributeParser
{
public:
  std::string name() const override { return "unit"; }

  std::unique_ptr<fibril_transport_common::AttributeBase> parse(AttributeParameter & param) override
  {
    if (param.values.size() != 1) {
      throw std::runtime_error("unit attribute must have exactly one value");
    }

    const auto & value = std::get<AttributeParameter::String>(param.values[0]);

    return std::make_unique<fibril_transport_common::UnitAttribute>(value.str);
  }
};

/**
 * @brief #[ros_frame_id("base_link")]
 * 
 * Specifies the ROS frame_id for coordinate frames.
 */
class RosFrameIdAttributeParser : public AttributeParser
{
public:
  std::string name() const override { return "ros_frame_id"; }

  std::unique_ptr<fibril_transport_common::AttributeBase> parse(AttributeParameter & param) override
  {
    if (param.values.size() != 1) {
      throw std::runtime_error("ros_frame_id attribute must have exactly one value");
    }

    const auto & value = std::get<AttributeParameter::String>(param.values[0]);

    return std::make_unique<fibril_transport_common::RosFrameIdAttribute>(value.str);
  }
};

/**
 * @brief Register all built-in attribute deserializers
 * 
 * This should be called once at program startup.
 */
inline void registerBuiltinAttributeParsers()
{
  fibril::AttributeParserRegistry::registerParser(std::make_unique<RosTypeAttributeParser>());
  fibril::AttributeParserRegistry::registerParser(std::make_unique<RosServiceAttributeParser>());
  fibril::AttributeParserRegistry::registerParser(std::make_unique<RosMapAttributeParser>());
  fibril::AttributeParserRegistry::registerParser(std::make_unique<RosAttributeParser>());
  fibril::AttributeParserRegistry::registerParser(std::make_unique<UnitAttributeParser>());
  fibril::AttributeParserRegistry::registerParser(std::make_unique<RosFrameIdAttributeParser>());
}

}  // namespace fibril

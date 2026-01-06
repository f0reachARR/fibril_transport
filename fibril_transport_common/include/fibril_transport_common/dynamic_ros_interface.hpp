#pragma once

#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <ros_babel_fish/babel_fish.hpp>
#include <ros_babel_fish/idl/type_support.hpp>
#include <ros_babel_fish/messages/compound_message.hpp>
#include <ros_babel_fish/messages/message.hpp>
#include <string>
#include <vector>

#include "definition_descriptors.hpp"

namespace fibril_transport_common
{

struct ArrayFieldTypeInfo;
struct CompoundFieldTypeInfo;

using FieldType = std::variant<
  PrimitiveType, std::shared_ptr<ArrayFieldTypeInfo>, std::shared_ptr<CompoundFieldTypeInfo>>;

FieldType messageToFieldType(const ros_babel_fish::Message & message);

struct ArrayFieldTypeInfo
{
  bool is_fixed;    // true if fixed size array
  bool is_bounded;  // true if bounded size (max_array_size > 0) array
  size_t array_size;
  size_t max_array_size;

  FieldType element_type_info;

  using Ptr = std::shared_ptr<ArrayFieldTypeInfo>;

  static Ptr from(const ros_babel_fish::Message & message);
};

struct CompoundFieldTypeInfo
{
  std::string type_name;

  using Ptr = std::shared_ptr<CompoundFieldTypeInfo>;

  static Ptr from(const ros_babel_fish::Message & message);
};

/**
 * @brief Field type information for codegen
 */
struct FieldTypeInfo
{
  std::string field_name;

  FieldType type_info;

  bool isPrimitive() const { return std::holds_alternative<PrimitiveType>(type_info); }

  bool isArray() const
  {
    return std::holds_alternative<std::shared_ptr<ArrayFieldTypeInfo>>(type_info);
  }

  bool isCompound() const
  {
    return std::holds_alternative<std::shared_ptr<CompoundFieldTypeInfo>>(type_info);
  }

  auto & asPrimitive() { return std::get<PrimitiveType>(type_info); }

  auto & asArray() { return std::get<std::shared_ptr<ArrayFieldTypeInfo>>(type_info); }

  auto & asCompound() { return std::get<std::shared_ptr<CompoundFieldTypeInfo>>(type_info); }

  const auto & asPrimitive() const { return std::get<PrimitiveType>(type_info); }

  const auto & asArray() const { return std::get<std::shared_ptr<ArrayFieldTypeInfo>>(type_info); }

  const auto & asCompound() const
  {
    return std::get<std::shared_ptr<CompoundFieldTypeInfo>>(type_info);
  }
};

/**
 * @brief Wrapper around ros_babel_fish for dynamic ROS message access
 * 
 * This class provides two main functionalities:
 * 1. Codegen: Type validation and field introspection at compile time
 * 2. Master: Dynamic message creation and manipulation at runtime
 */
class DynamicRosInterface
{
public:
  DynamicRosInterface();
  ~DynamicRosInterface();

  // ========== Codegen side API ==========

  /**
   * @brief Get message type support
   * @param type_name Message type (e.g., "geometry_msgs/msg/Twist")
   * @return MessageTypeSupport if found
   * @throws std::runtime_error if type not found
   */
  ros_babel_fish::MessageTypeSupport::ConstSharedPtr getMessageTypeSupport(
    const std::string & type_name) const;

  /**
   * @brief Get service type support
   * @param type_name Service type (e.g., "std_srvs/srv/SetBool")
   * @return ServiceTypeSupport if found
   * @throws std::runtime_error if type not found
   */
  ros_babel_fish::ServiceTypeSupport::ConstSharedPtr getServiceTypeSupport(
    const std::string & type_name) const;

  /**
   * @brief Get message members introspection
   * @param type_name Message type (e.g., "geometry_msgs/msg/Twist")
   * @return MessageMembersIntrospection if found
   * @throws std::runtime_error if type not found
   */
  ros_babel_fish::MessageMembersIntrospection getMessageMembersIntrospection(
    ros_babel_fish::MessageTypeSupport::ConstSharedPtr type_support) const;

  /**
   * @brief Get service members introspection
   * @param type_name Service type (e.g., "std_srvs/srv/SetBool")
   * @return ServiceMembersIntrospection if found
   * @throws std::runtime_error if type not found
   */
  ros_babel_fish::MessageMembersIntrospection getServiceRequestMembersIntrospection(
    ros_babel_fish::ServiceTypeSupport::ConstSharedPtr type_support) const;

  /**
   * @brief Get service members introspection
   * @param type_name Service type (e.g., "std_srvs/srv/SetBool")
   * @return ServiceMembersIntrospection if found
   * @throws std::runtime_error if type not found
   */
  ros_babel_fish::MessageMembersIntrospection getServiceResponseMembersIntrospection(
    ros_babel_fish::ServiceTypeSupport::ConstSharedPtr type_support) const;

  /**
   * @brief Validate if a field path exists in a message type
   * @param type_name Message type name
   * @param field_path Dot-separated field path (e.g., "linear.x")
   * @return true if field path is valid, false otherwise
   */
  bool validateFieldPath(
    const ros_babel_fish::MessageMembersIntrospection members,
    const std::string & field_path) const;

  /**
   * @brief Get detailed type information for a specific field
   * @param type_name Message type name
   * @param field_path Dot-separated field path
   * @return Field type information
   * @throws std::runtime_error if field not found
   */
  FieldTypeInfo getFieldTypeInfo(
    const ros_babel_fish::MessageMembersIntrospection members,
    const std::string & field_path) const;

  /**
   * @brief Get list of all fields at a specific hierarchy level (for LSP completion)
   * @param type_name Message type name
   * @param field_path Dot-separated path to the hierarchy level (empty for top level)
   * @return Vector of field type information
   * 
   * Example:
   *   getFieldList("geometry_msgs/msg/Twist", "") -> [linear, angular]
   *   getFieldList("geometry_msgs/msg/Twist", "linear") -> [x, y, z]
   */
  std::vector<FieldTypeInfo> getFieldList(
    const ros_babel_fish::MessageMembersIntrospection members,
    const std::string & field_path) const;

  // ========== Master side API ==========

  /**
   * @brief Create an empty message of the given type
   * @param type_name Message type
   * @return Shared pointer to the created message
   * @throws std::runtime_error if type not found
   */
  ros_babel_fish::CompoundMessage::SharedPtr createMessage(const std::string & type_name) const;

  /**
   * @brief Create en empty message of the given MessageTypeSupport
   * @param type_support Message type support
   * @return Shared pointer to the created message
   * @throws std::runtime_error if type not found
   */
  ros_babel_fish::CompoundMessage::SharedPtr createMessage(
    const ros_babel_fish::MessageTypeSupport::ConstSharedPtr & type_support) const;

  /**
   * @brief Create en empty message of the given MessageMembersIntrospection
   * @param members Message members introspection
   * @return Shared pointer to the created message
   * @throws std::runtime_error if type not found
   */
  ros_babel_fish::CompoundMessage::SharedPtr createMessage(
    const ros_babel_fish::MessageMembersIntrospection members) const;

  /**
   * @brief Set field value in a message
   * @param message Target message
   * @param field_path Dot-separated field path
   * @param value Value to set (as variant or template)
   * @throws std::runtime_error if field not found or type mismatch
   * 
   * Note: Template specializations for common types (int, float, double, etc.)
   */
  template <typename T>
  void setFieldValue(
    ros_babel_fish::CompoundMessage & message, const std::string & field_path,
    const T & value) const;

  /**
   * @brief Get field value from a message
   * @param message Source message
   * @param field_path Dot-separated field path
   * @return Field value
   * @throws std::runtime_error if field not found
   */
  template <typename T>
  T getFieldValue(
    const ros_babel_fish::CompoundMessage & message, const std::string & field_path) const;

  /**
   * @brief Create a service client
   * @param node ROS node
   * @param service_name Service name
   * @param service_type Service type
   * @return Service client
   * @throws std::runtime_error if service type not found
   */
  ros_babel_fish::BabelFishServiceClient::SharedPtr createServiceClient(
    rclcpp::Node & node, const std::string & service_name, const std::string & service_type) const;

  /**
   * @brief Create a service server
   * @param node ROS node
   * @param service_name Service name
   * @param service_type Service type
   * @param callback Service callback
   * @return Service server
   * @throws std::runtime_error if service type not found
   */
  ros_babel_fish::BabelFishService::SharedPtr createService(
    rclcpp::Node & node, const std::string & service_name, const std::string & service_type,
    ros_babel_fish::AnyServiceCallback callback) const;

private:
  std::unique_ptr<ros_babel_fish::BabelFish> babel_fish_;

  /**
   * @brief Navigate to a field in a message using a dot-separated path
   * @param message Root message
   * @param paths Field path
   * @return Reference to the target field
   * @throws std::runtime_error if field not found
   */
  ros_babel_fish::Message & navigateToField(
    ros_babel_fish::CompoundMessage & message, const std::vector<std::string> & paths) const;

  const ros_babel_fish::Message & navigateToField(
    const ros_babel_fish::CompoundMessage & message, const std::vector<std::string> & paths) const;

  /**
   * @brief Navigate to a field in a message using a dot-separated path
   * @param message Root message
   * @param field_path Field path
   * @return Reference to the target field
   * @throws std::runtime_error if field not found
   */
  ros_babel_fish::Message & navigateToField(
    ros_babel_fish::CompoundMessage & message, const std::string & field_path) const;

  const ros_babel_fish::Message & navigateToField(
    const ros_babel_fish::CompoundMessage & message, const std::string & field_path) const;

  /**
   * @brief Split field path by dots
   */
  std::vector<std::string> splitFieldPath(const std::string & field_path) const;
};

}  // namespace fibril_transport_common

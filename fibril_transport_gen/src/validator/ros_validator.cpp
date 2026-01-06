#include "ros_validator.hpp"

#include <fibril_transport_common/dynamic_ros_interface.hpp>
#include <fibril_transport_common/ros_attribute.hpp>

#include "validator/validator.hpp"

using namespace fibril_transport_common;

namespace fibril
{

RosValidator::RosValidator()
{
  try {
    ros_interface_ = std::make_unique<DynamicRosInterface>();
  } catch (const std::exception & e) {
    throw std::runtime_error(
      "Failed to initialize ROS interface for validation. "
      "Make sure ROS 2 is properly sourced and ros_babel_fish is available. Error: " +
      std::string(e.what()));
  }
}

RosValidator::~RosValidator() = default;

bool RosValidator::validate(const SourceFile & source, std::vector<ValidationError> & errors)
{
  bool all_valid = true;

  // Validate all structs with ros_type or ros_map attributes
  for (const auto & struct_def : source.structs) {
    if (!validateRosTypeAttribute(struct_def, source, errors)) {
      all_valid = false;
    }
    if (!validateRosMapAttributes(struct_def, source, errors)) {
      all_valid = false;
    }
  }

  // Validate all nodes (check services)
  for (const auto & node_def : source.nodes) {
    if (!validateNode(node_def, source, errors)) {
      all_valid = false;
    }
  }

  return all_valid;
}

bool RosValidator::validateRosTypeAttribute(
  const AstStructDescriptor & struct_def, const SourceFile & source,
  std::vector<ValidationError> & errors)
{
  auto * ros_type_attr = struct_def.attributes.find<RosTypeAttribute>();
  if (!ros_type_attr) {
    return true;
  }

  const std::string & ros_type = ros_type_attr->ros_message_type;

  try {
    ros_interface_->getMessageTypeSupport(ros_type);
    return true;
  } catch (const std::exception & e) {
    reportError(
      "Invalid ROS message type '" + ros_type + "' in struct '" + struct_def.name +
        "': " + std::string(e.what()),
      struct_def.line, struct_def.column, source.file_path, errors);
    return false;
  }
}

bool RosValidator::validateRosMapAttributes(
  const AstStructDescriptor & struct_def, const SourceFile & source,
  std::vector<ValidationError> & errors)
{
  auto * ros_type_attr = struct_def.attributes.find<RosTypeAttribute>();
  if (!ros_type_attr) {
    return true;
  }

  const std::string & ros_type = ros_type_attr->ros_message_type;
  bool all_valid = true;

  try {
    auto type_support = ros_interface_->getMessageTypeSupport(ros_type);
    auto members = ros_interface_->getMessageMembersIntrospection(type_support);

    for (const auto & field : struct_def.fields) {
      auto * ros_map_attr = field.attributes.find<RosMapAttribute>();
      if (!ros_map_attr) {
        continue;
      }

      const std::string & field_path = ros_map_attr->field_path;

      try {
        if (!ros_interface_->validateFieldPath(members, field_path)) {
          reportError(
            "Invalid field path '" + field_path + "' in ROS message type '" + ros_type +
              "' for field '" + field.name + "'",
            struct_def.line, struct_def.column, source.file_path, errors);
          all_valid = false;
        }
      } catch (const std::exception & e) {
        reportError(
          "Error validating field path '" + field_path + "' for field '" + field.name +
            "': " + std::string(e.what()),
          struct_def.line, struct_def.column, source.file_path, errors);
        all_valid = false;
      }
    }
  } catch (const std::exception & e) {
    reportError(
      "Error getting message introspection for type '" + ros_type + "': " + std::string(e.what()),
      struct_def.line, struct_def.column, source.file_path, errors);
    return false;
  }

  return all_valid;
}

bool RosValidator::validateNode(
  const NodeDefinition & node_def, const SourceFile & source, std::vector<ValidationError> & errors)
{
  bool all_valid = true;

  for (const auto & port : node_def.ports) {
    std::visit(
      overload{
        [&](const ServicePort & service_port) -> void {
          if (!validateRosServiceAttribute(service_port, source, errors)) {
            all_valid = false;
          }
        },
        [](const SubPort & sub_port) -> void {},
        [](const PubPort & pub_port) -> void {},
      },
      port.get());
  }

  return all_valid;
}

bool RosValidator::validateRosPubSubTypeAttribute(
  const TypeDescriptor & type_descriptor, const SourceFile & source,
  std::vector<ValidationError> & errors)
{
  if (!type_descriptor.isStruct()) {
    reportError("ROS pub/sub must use Struct", 0, 0, source.file_path, errors);
    return false;
  }
  const auto * struct_def = source.findStruct(type_descriptor.asStructName());
  assert(struct_def);

  auto * type_attribute = struct_def->attributes.find<RosTypeAttribute>();
  if (!type_attribute) {
    reportError(
      "ROS pub/sub must use Struct with ros_type attribute", 0, 0, source.file_path, errors);
    return false;
  }

  return true;
}

bool RosValidator::validateRosServiceAttribute(
  const ServicePort & service_port, const SourceFile & source,
  std::vector<ValidationError> & errors)
{
  auto * ros_service_attr = service_port.attributes.find<RosServiceAttribute>();
  if (!ros_service_attr) {
    return true;
  }

  const std::string & service_type = ros_service_attr->ros_service_type;

  try {
    ros_interface_->getServiceTypeSupport(service_type);
    return validateServiceRequestResponseMapping(service_port, service_type, source, errors);
  } catch (const std::exception & e) {
    reportError(
      "Invalid ROS service type '" + service_type + "' in service port '" + service_port.name +
        "': " + std::string(e.what()),
      0, 0, source.file_path, errors);
    return false;
  }
}

bool RosValidator::validateServiceRequestResponseMapping(
  const ServicePort & service_port, const std::string & service_type, const SourceFile & source,
  std::vector<ValidationError> & errors)
{
  bool all_valid = true;

  try {
    auto service_type_support = ros_interface_->getServiceTypeSupport(service_type);

    // Validate request type
    if (service_port.request_type.isStruct()) {
      const std::string & struct_name = service_port.request_type.asStructName();
      const auto * request_struct = source.findStruct(struct_name);

      assert(request_struct);

      auto request_members =
        ros_interface_->getServiceRequestMembersIntrospection(service_type_support);

      for (const auto & field : request_struct->fields) {
        auto * ros_map_attr = field.attributes.find<RosMapAttribute>();
        std::string field_path = ros_map_attr ? ros_map_attr->field_path : field.name;

        if (!ros_interface_->validateFieldPath(request_members, field_path)) {
          reportError(
            "Field '" + field.name + "' maps to invalid path '" + field_path +
              "' in service request type '" + service_type + "'",
            request_struct->line, request_struct->column, source.file_path, errors);
          all_valid = false;
        }
      }
    } else {
      reportError(
        "Service request type '" + service_port.request_type.asStructName() + "' is not a Struct",
        0, 0, source.file_path, errors);
      all_valid = false;
    }

    // Validate response type
    if (service_port.response_type.isStruct()) {
      const std::string & struct_name = service_port.response_type.asStructName();
      const auto * response_struct = source.findStruct(struct_name);

      assert(response_struct);

      auto response_members =
        ros_interface_->getServiceResponseMembersIntrospection(service_type_support);

      for (const auto & field : response_struct->fields) {
        auto * ros_map_attr = field.attributes.find<RosMapAttribute>();
        std::string field_path = ros_map_attr ? ros_map_attr->field_path : field.name;

        if (!ros_interface_->validateFieldPath(response_members, field_path)) {
          reportError(
            "Field '" + field.name + "' maps to invalid path '" + field_path +
              "' in service response type '" + service_type + "'",
            response_struct->line, response_struct->column, source.file_path, errors);
          all_valid = false;
        }
      }
    } else {
      reportError(
        "Service response type '" + service_port.response_type.asStructName() + "' is not a Struct",
        0, 0, source.file_path, errors);
      all_valid = false;
    }
  } catch (const std::exception & e) {
    reportError(
      "Error getting service introspection for type '" + service_type +
        "': " + std::string(e.what()),
      0, 0, source.file_path, errors);
    return false;
  }

  return all_valid;
}

void RosValidator::reportError(
  const std::string & message, size_t line, size_t column, const std::string & file,
  std::vector<ValidationError> & errors)
{
  ValidationError error;
  error.message = message;
  error.line = line;
  error.column = column;
  error.file_path = file;
  errors.push_back(error);
}

}  // namespace fibril

#include "ros_validator.hpp"

#include <fibril_transport_common/dynamic_ros_interface.hpp>

#include "validator/validator.hpp"

namespace fibril
{

RosValidator::RosValidator()
{
  try {
    ros_interface_ = std::make_unique<fibril_transport_common::DynamicRosInterface>();
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
  const StructDefinition & struct_def, const SourceFile & source,
  std::vector<ValidationError> & errors)
{
  auto ros_type_opt = getAttributeValue(struct_def.attributes, "ros_type");
  if (!ros_type_opt) {
    // No ros_type attribute, nothing to validate
    return true;
  }

  const std::string & ros_type = *ros_type_opt;

  try {
    // Attempt to get message type support
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
  const StructDefinition & struct_def, const SourceFile & source,
  std::vector<ValidationError> & errors)
{
  // First, check if struct has ros_type attribute
  auto ros_type_opt = getAttributeValue(struct_def.attributes, "ros_type");
  if (!ros_type_opt) {
    // No ros_type attribute - this struct might be used for services
    // Service request/response structs are validated separately in validateServiceRequestResponseMapping
    // So we skip validation here
    return true;
  }

  const std::string & ros_type = *ros_type_opt;
  bool all_valid = true;

  // Get message type support and members introspection
  try {
    auto type_support = ros_interface_->getMessageTypeSupport(ros_type);
    auto members = ros_interface_->getMessageMembersIntrospection(type_support);

    // Validate each field's ros_map attribute
    for (const auto & field : struct_def.fields) {
      auto ros_map_opt = getAttributeValue(field.attributes, "ros_map");
      if (!ros_map_opt) {
        // No ros_map, skip (field name will be used as-is)
        continue;
      }

      const std::string & field_path = *ros_map_opt;

      try {
        // Validate the field path exists in the ROS message type
        if (!ros_interface_->validateFieldPath(members, field_path)) {
          reportError(
            "Invalid field path '" + field_path + "' in ROS message type '" + ros_type +
              "' for field '" + field.name + "'",
            field.line, field.column, source.file_path, errors);
          all_valid = false;
        }
      } catch (const std::exception & e) {
        reportError(
          "Error validating field path '" + field_path + "' for field '" + field.name +
            "': " + std::string(e.what()),
          field.line, field.column, source.file_path, errors);
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

  // Check each port for service-related validation
  for (const auto & port : node_def.ports) {
    // Only validate service ports
    if (std::holds_alternative<ServicePort>(port)) {
      const auto & service_port = std::get<ServicePort>(port);
      if (!validateRosServiceAttribute(service_port, source, errors)) {
        all_valid = false;
      }
    }
  }

  return all_valid;
}

bool RosValidator::validateRosServiceAttribute(
  const ServicePort & service_port, const SourceFile & source,
  std::vector<ValidationError> & errors)
{
  auto ros_service_opt = getAttributeValue(service_port.attributes, "ros_service");
  if (!ros_service_opt) {
    // No ros_service attribute, nothing to validate
    return true;
  }

  const std::string & service_type = *ros_service_opt;

  try {
    // Attempt to get service type support
    ros_interface_->getServiceTypeSupport(service_type);

    // Also validate request/response mapping
    return validateServiceRequestResponseMapping(service_port, service_type, source, errors);

  } catch (const std::exception & e) {
    reportError(
      "Invalid ROS service type '" + service_type + "' in service port '" + service_port.name +
        "': " + std::string(e.what()),
      service_port.line, service_port.column, source.file_path, errors);
    return false;
  }
}

bool RosValidator::validateServiceRequestResponseMapping(
  const ServicePort & service_port, const std::string & service_type, const SourceFile & source,
  std::vector<ValidationError> & errors)
{
  bool all_valid = true;

  try {
    // Get service type support
    auto service_type_support = ros_interface_->getServiceTypeSupport(service_type);

    // Validate request type
    if (service_port.request_type.isStruct()) {
      const auto & struct_type = service_port.request_type.asStruct();
      const auto * request_struct = source.findStruct(struct_type.name);

      if (request_struct) {
        // Get request members introspection
        auto request_members =
          ros_interface_->getServiceRequestMembersIntrospection(service_type_support);

        // Validate each field maps correctly to service request fields
        for (const auto & field : request_struct->fields) {
          auto ros_map_opt = getAttributeValue(field.attributes, "ros_map");
          if (!ros_map_opt) {
            // If no ros_map, use field name directly - still need to validate
            if (!ros_interface_->validateFieldPath(request_members, field.name)) {
              reportError(
                "Field '" + field.name + "' does not exist in service request type '" +
                  service_type + "'. Add #[ros_map(...)] to map to correct field.",
                field.line, field.column, source.file_path, errors);
              all_valid = false;
            }
            continue;
          }

          const std::string & field_path = *ros_map_opt;

          try {
            // Validate the field path exists in service request
            if (!ros_interface_->validateFieldPath(request_members, field_path)) {
              reportError(
                "Invalid field path '" + field_path + "' in service request type '" + service_type +
                  "' for field '" + field.name + "'",
                field.line, field.column, source.file_path, errors);
              all_valid = false;
            }
          } catch (const std::exception & e) {
            reportError(
              "Error validating service request field '" + field.name +
                "': " + std::string(e.what()),
              field.line, field.column, source.file_path, errors);
            all_valid = false;
          }
        }
      }
    }

    // Validate response type
    if (service_port.response_type.isStruct()) {
      const auto & struct_type = service_port.response_type.asStruct();
      const auto * response_struct = source.findStruct(struct_type.name);

      if (response_struct) {
        // Get response members introspection
        auto response_members =
          ros_interface_->getServiceResponseMembersIntrospection(service_type_support);

        // Validate each field maps correctly to service response fields
        for (const auto & field : response_struct->fields) {
          auto ros_map_opt = getAttributeValue(field.attributes, "ros_map");
          if (!ros_map_opt) {
            // If no ros_map, use field name directly - still need to validate
            if (!ros_interface_->validateFieldPath(response_members, field.name)) {
              reportError(
                "Field '" + field.name + "' does not exist in service response type '" +
                  service_type + "'. Add #[ros_map(...)] to map to correct field.",
                field.line, field.column, source.file_path, errors);
              all_valid = false;
            }
            continue;
          }

          const std::string & field_path = *ros_map_opt;

          try {
            // Validate the field path exists in service response
            if (!ros_interface_->validateFieldPath(response_members, field_path)) {
              reportError(
                "Invalid field path '" + field_path + "' in service response type '" +
                  service_type + "' for field '" + field.name + "'",
                field.line, field.column, source.file_path, errors);
              all_valid = false;
            }
          } catch (const std::exception & e) {
            reportError(
              "Error validating service response field '" + field.name +
                "': " + std::string(e.what()),
              field.line, field.column, source.file_path, errors);
            all_valid = false;
          }
        }
      }
    }
  } catch (const std::exception & e) {
    reportError(
      "Error getting service introspection for type '" + service_type +
        "': " + std::string(e.what()),
      service_port.line, service_port.column, source.file_path, errors);
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

std::optional<std::string> RosValidator::getAttributeValue(
  const std::vector<Attribute> & attributes, const std::string & attr_name) const
{
  for (const auto & attr : attributes) {
    if (attr.name == attr_name) {
      return attr.getStringArg(0);
    }
  }
  return std::nullopt;
}

}  // namespace fibril

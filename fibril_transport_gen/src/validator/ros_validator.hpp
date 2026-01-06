#pragma once

#include <fibril_transport_common/dynamic_ros_interface.hpp>
#include <memory>
#include <string>
#include <vector>

#include "parser/ast.hpp"

namespace fibril
{

// Forward declaration
struct ValidationError;

/**
 * @brief ROS type validation using dynamic_ros_interface
 * 
 * This validator checks DSL attributes against actual ROS type definitions:
 * - ros_type: Validates message/service type exists
 * - ros_map: Validates field paths exist in ROS message
 * - ros_service: Validates service type and request/response mappings
 */
class RosValidator
{
public:
  RosValidator();
  ~RosValidator();

  /**
   * @brief Validate all ROS-related attributes in a source file
   * @param source Source file to validate
   * @param errors Output vector for validation errors
   * @return true if validation passed, false otherwise
   */
  bool validate(const SourceFile & source, std::vector<ValidationError> & errors);

private:
  std::unique_ptr<fibril_transport_common::DynamicRosInterface> ros_interface_;

  // Struct validation
  bool validateRosTypeAttribute(
    const AstStructDescriptor & struct_def, const SourceFile & source,
    std::vector<ValidationError> & errors);

  bool validateRosMapAttributes(
    const AstStructDescriptor & struct_def, const SourceFile & source,
    std::vector<ValidationError> & errors);

  // Node validation
  bool validateNode(
    const NodeDefinition & node_def, const SourceFile & source,
    std::vector<ValidationError> & errors);

  // Pub/Sub validation
  bool validateRosPubSubTypeAttribute(
    const TypeDescriptor & type_descriptor, const SourceFile & source,
    std::vector<ValidationError> & errors);

  // Service validation
  bool validateRosServiceAttribute(
    const ServicePort & service_port, const SourceFile & source,
    std::vector<ValidationError> & errors);

  bool validateServiceRequestResponseMapping(
    const ServicePort & service_port, const std::string & service_type, const SourceFile & source,
    std::vector<ValidationError> & errors);

  // Helper methods
  void reportError(
    const std::string & message, size_t line, size_t column, const std::string & file,
    std::vector<ValidationError> & errors);
};

}  // namespace fibril

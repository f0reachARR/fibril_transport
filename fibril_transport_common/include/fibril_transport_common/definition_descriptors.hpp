#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "attribute_system.hpp"
#include "type_system.hpp"

namespace fibril_transport_common
{

/**
 * @brief Port direction (for binary serialization)
 */
enum class PortDirection : uint8_t {
  Sub = 0,     // Device subscribes (receives)
  Pub = 1,     // Device publishes (sends)
  Service = 2  // Bidirectional service
};

// ========================================
// Port Types (variant pattern)
// ========================================

/**
 * @brief Sub port: Device subscribes (receives from Master)
 * 
 * Represents a command port where the device receives data from the master.
 */
struct SubPort
{
  std::string name;
  TypeDescriptor data_type;
  AttributeList attributes;

  explicit SubPort(std::string name, TypeDescriptor type)
  : name(std::move(name)), data_type(std::move(type))
  {
  }

  explicit SubPort(std::string name, TypeDescriptor type, AttributeList attrs)
  : name(std::move(name)), data_type(std::move(type)), attributes(std::move(attrs))
  {
  }
};

/**
 * @brief Pub port: Device publishes (sends to Master)
 * 
 * Represents a notification port where the device sends data to the master.
 */
struct PubPort
{
  std::string name;
  TypeDescriptor data_type;
  AttributeList attributes;

  explicit PubPort(std::string name, TypeDescriptor type)
  : name(std::move(name)), data_type(std::move(type))
  {
  }

  explicit PubPort(std::string name, TypeDescriptor type, AttributeList attrs)
  : name(std::move(name)), data_type(std::move(type)), attributes(std::move(attrs))
  {
  }
};

/**
 * @brief Service port: Bidirectional request/response
 * 
 * Represents a service with request and response types.
 */
struct ServicePort
{
  std::string name;
  TypeDescriptor request_type;
  TypeDescriptor response_type;
  AttributeList attributes;

  explicit ServicePort(std::string name, TypeDescriptor request_type, TypeDescriptor response_type)
  : name(std::move(name)),
    request_type(std::move(request_type)),
    response_type(std::move(response_type))
  {
  }

  explicit ServicePort(
    std::string name, TypeDescriptor request_type, TypeDescriptor response_type,
    AttributeList attrs)
  : name(std::move(name)),
    request_type(std::move(request_type)),
    response_type(std::move(response_type)),
    attributes(std::move(attrs))
  {
  }
};

/**
 * @brief Port variant type
 * 
 * A port can be one of: SubPort, PubPort, or ServicePort
 */
using Port = std::variant<SubPort, PubPort, ServicePort>;

using PortRef = std::variant<SubPort &, PubPort &, ServicePort &>;

// ========================================
// Port Helper Functions
// ========================================

/**
 * @brief Get port name (works for any port type)
 */
const std::string & getPortName(const Port & port);

/**
 * @brief Get port attributes (works for any port type)
 */
const AttributeList & getPortAttributes(const Port & port);

/**
 * @brief Get mutable port attributes (works for any port type)
 */
AttributeList & getPortAttributesMut(Port & port);

/**
 * @brief Get port direction
 */
PortDirection getPortDirection(const Port & port);

// ========================================
// Node Descriptor
// ========================================

/**
 * @brief Node descriptor (complete node definition)
 * 
 * This is the unified representation used by:
 * - Parser (after converting from AST)
 * - Binary serialization (definition.bin)
 * - Master (after deserializing from binary)
 */
struct NodeDescriptor
{
  std::string name;
  std::vector<Port> ports;
  std::vector<StructDescriptor> structs;
  AttributeList attributes;

  /**
     * @brief Find port by name
     * @return Pointer to port, or nullptr if not found
     */
  const Port * findPort(const std::string & name) const;

  /**
     * @brief Find struct by name
     * @return Pointer to struct, or nullptr if not found
     */
  const StructDescriptor * findStruct(const std::string & name) const;

  /**
     * @brief Serialize to definition.bin format
     * 
     * Creates a complete definition.bin binary according to the spec
     * in main.md Appendix B.
     * 
     * @return Binary representation (ready to embed in firmware)
     */
  std::vector<uint8_t> serializeToBinary() const;

  /**
     * @brief Deserialize from definition.bin format
     * 
     * @param data Binary data in definition.bin format
     * @return Deserialized node descriptor
     */
  static NodeDescriptor deserializeFromBinary(const std::vector<uint8_t> & data);

  /**
     * @brief Calculate checksum (CRC32 of binary representation)
     * 
     * This checksum is used by the Master to cache and identify node definitions.
     * 
     * @return CRC32 checksum
     */
  uint32_t calculateChecksum() const;
};

}  // namespace fibril_transport_common

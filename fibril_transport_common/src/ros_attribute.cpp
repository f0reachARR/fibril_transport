#include "fibril_transport_common/ros_attribute.hpp"

#include <cstring>
#include <map>
#include <stdexcept>

#include "binary.hpp"
#include "fibril_transport_common/attribute_system.hpp"

namespace fibril_transport_common
{

// ========================================
// RosTypeAttribute Implementation
// ========================================

RosTypeAttribute::RosTypeAttribute(std::string type) : ros_message_type(std::move(type)) {}

std::string RosTypeAttribute::name() const { return "ros_type"; }

std::vector<uint8_t> RosTypeAttribute::serializeToBinary() const
{
  std::vector<uint8_t> buffer;
  writeString(buffer, ros_message_type);
  return buffer;
}

std::unique_ptr<RosTypeAttribute> RosTypeAttribute::fromBinary(const std::vector<uint8_t> & data)
{
  size_t offset = 0;
  std::string type = readString(data, offset, data.size());
  return std::make_unique<RosTypeAttribute>(std::move(type));
}

// ========================================
// RosServiceAttribute Implementation
// ========================================

RosServiceAttribute::RosServiceAttribute(std::string type) : ros_service_type(std::move(type)) {}

std::string RosServiceAttribute::name() const { return "ros_service"; }

std::vector<uint8_t> RosServiceAttribute::serializeToBinary() const
{
  std::vector<uint8_t> buffer;
  writeString(buffer, ros_service_type);
  return buffer;
}

std::unique_ptr<RosServiceAttribute> RosServiceAttribute::fromBinary(
  const std::vector<uint8_t> & data)
{
  size_t offset = 0;
  std::string type = readString(data, offset, data.size());
  return std::make_unique<RosServiceAttribute>(std::move(type));
}

// ========================================
// RosMapAttribute Implementation
// ========================================

RosMapAttribute::RosMapAttribute(std::string path) : field_path(std::move(path)) {}

std::string RosMapAttribute::name() const { return "ros_map"; }

std::vector<uint8_t> RosMapAttribute::serializeToBinary() const
{
  std::vector<uint8_t> buffer;
  writeString(buffer, field_path);
  return buffer;
}

std::unique_ptr<RosMapAttribute> RosMapAttribute::fromBinary(const std::vector<uint8_t> & data)
{
  size_t offset = 0;
  std::string path = readString(data, offset, data.size());
  return std::make_unique<RosMapAttribute>(std::move(path));
}

// ========================================
// RosAttribute Implementation
// ========================================

RosAttribute::RosAttribute(std::string name) : topic_name(std::move(name)) {}

std::string RosAttribute::name() const { return "ros"; }

std::vector<uint8_t> RosAttribute::serializeToBinary() const
{
  std::vector<uint8_t> buffer;
  writeString(buffer, topic_name);
  return buffer;
}

std::unique_ptr<RosAttribute> RosAttribute::fromBinary(const std::vector<uint8_t> & data)
{
  size_t offset = 0;
  std::string name = readString(data, offset, data.size());
  return std::make_unique<RosAttribute>(std::move(name));
}

// ========================================
// UnitAttribute Implementation
// ========================================

UnitAttribute::UnitAttribute(std::string u) : unit(std::move(u)) {}

std::string UnitAttribute::name() const { return "unit"; }

std::vector<uint8_t> UnitAttribute::serializeToBinary() const
{
  std::vector<uint8_t> buffer;
  writeString(buffer, unit);
  return buffer;
}

std::unique_ptr<UnitAttribute> UnitAttribute::fromBinary(const std::vector<uint8_t> & data)
{
  size_t offset = 0;
  std::string u = readString(data, offset, data.size());
  return std::make_unique<UnitAttribute>(std::move(u));
}

// ========================================
// RosFrameIdAttribute Implementation
// ========================================

RosFrameIdAttribute::RosFrameIdAttribute(std::string id) : frame_id(std::move(id)) {}

std::string RosFrameIdAttribute::name() const { return "ros_frame_id"; }

std::vector<uint8_t> RosFrameIdAttribute::serializeToBinary() const
{
  std::vector<uint8_t> buffer;
  writeString(buffer, frame_id);
  return buffer;
}

std::unique_ptr<RosFrameIdAttribute> RosFrameIdAttribute::fromBinary(
  const std::vector<uint8_t> & data)
{
  size_t offset = 0;
  std::string id = readString(data, offset, data.size());
  return std::make_unique<RosFrameIdAttribute>(std::move(id));
}

// ========================================
// Built-in Registration
// ========================================

void registerBuiltinAttributes()
{
  AttributeFactory::registerDeserializer(
    "ros_type", [](const std::vector<uint8_t> & data) -> std::unique_ptr<AttributeBase> {
      return RosTypeAttribute::fromBinary(data);
    });

  AttributeFactory::registerDeserializer(
    "ros_map", [](const std::vector<uint8_t> & data) -> std::unique_ptr<AttributeBase> {
      return RosMapAttribute::fromBinary(data);
    });

  AttributeFactory::registerDeserializer(
    "ros", [](const std::vector<uint8_t> & data) -> std::unique_ptr<AttributeBase> {
      return RosAttribute::fromBinary(data);
    });

  AttributeFactory::registerDeserializer(
    "unit", [](const std::vector<uint8_t> & data) -> std::unique_ptr<AttributeBase> {
      return UnitAttribute::fromBinary(data);
    });

  AttributeFactory::registerDeserializer(
    "ros_frame_id", [](const std::vector<uint8_t> & data) -> std::unique_ptr<AttributeBase> {
      return RosFrameIdAttribute::fromBinary(data);
    });

  AttributeFactory::registerDeserializer(
    "ros_service", [](const std::vector<uint8_t> & data) -> std::unique_ptr<AttributeBase> {
      return RosServiceAttribute::fromBinary(data);
    });
}

}  // namespace fibril_transport_common

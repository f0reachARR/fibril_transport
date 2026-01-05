#include "fibril_transport_common/attribute_system.hpp"

#include <cstring>
#include <map>
#include <stdexcept>

namespace fibril_transport_common
{

// ========================================
// Binary Serialization Utilities
// ========================================

namespace
{

// Write a uint8_t
void writeUInt8(std::vector<uint8_t> & buffer, uint8_t value) { buffer.push_back(value); }

// Write a uint16_t (little-endian)
void writeUInt16(std::vector<uint8_t> & buffer, uint16_t value)
{
  buffer.push_back(static_cast<uint8_t>(value & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

// Write a string with length prefix
void writeString(std::vector<uint8_t> & buffer, const std::string & str)
{
  buffer.insert(buffer.end(), str.begin(), str.end());
}

// Read a uint8_t
uint8_t readUInt8(const std::vector<uint8_t> & data, size_t & offset)
{
  if (offset >= data.size()) {
    throw std::runtime_error("Buffer underflow reading uint8");
  }
  return data[offset++];
}

// Read a uint16_t (little-endian)
uint16_t readUInt16(const std::vector<uint8_t> & data, size_t & offset)
{
  if (offset + 1 >= data.size()) {
    throw std::runtime_error("Buffer underflow reading uint16");
  }
  uint16_t value = data[offset] | (static_cast<uint16_t>(data[offset + 1]) << 8);
  offset += 2;
  return value;
}

// Read a string of given length
std::string readString(const std::vector<uint8_t> & data, size_t & offset, size_t length)
{
  if (offset + length > data.size()) {
    throw std::runtime_error("Buffer underflow reading string");
  }
  std::string str(reinterpret_cast<const char *>(&data[offset]), length);
  offset += length;
  return str;
}

}  // anonymous namespace

// ========================================
// AttributeList Implementation
// ========================================

void AttributeList::add(std::unique_ptr<AttributeBase> attr)
{
  attributes_.push_back(std::move(attr));
}

std::vector<uint8_t> AttributeList::serializeToBinary() const
{
  std::vector<uint8_t> buffer;

  // Write count
  writeUInt8(buffer, static_cast<uint8_t>(attributes_.size()));

  // Write each attribute
  for (const auto & attr : attributes_) {
    std::string key = attr->metadataKey();
    std::vector<uint8_t> value = attr->serializeToBinary();

    // Write key length and key
    writeUInt8(buffer, static_cast<uint8_t>(key.size()));
    writeString(buffer, key);

    // Write value length and value
    writeUInt16(buffer, static_cast<uint16_t>(value.size()));
    buffer.insert(buffer.end(), value.begin(), value.end());
  }

  return buffer;
}

AttributeList AttributeList::deserializeFromBinary(
  const std::vector<uint8_t> & data, size_t & offset)
{
  AttributeList list;

  // Read count
  uint8_t count = readUInt8(data, offset);

  // Read each attribute
  for (uint8_t i = 0; i < count; ++i) {
    // Read key
    uint8_t key_len = readUInt8(data, offset);
    std::string key = readString(data, offset, key_len);

    // Read value
    uint16_t value_len = readUInt16(data, offset);
    std::vector<uint8_t> value(data.begin() + offset, data.begin() + offset + value_len);
    offset += value_len;

    // Deserialize attribute
    auto attr = AttributeFactory::fromBinary(key, value);
    if (attr) {
      list.add(std::move(attr));
    }
  }

  return list;
}

// ========================================
// AttributeFactory Implementation
// ========================================

std::map<std::string, AttributeFactory::DeserializerFunc> & AttributeFactory::getDeserializers()
{
  static std::map<std::string, DeserializerFunc> deserializers;
  return deserializers;
}

std::unique_ptr<AttributeBase> AttributeFactory::fromBinary(
  const std::string & key, const std::vector<uint8_t> & value)
{
  auto & deserializers = getDeserializers();
  auto it = deserializers.find(key);
  if (it != deserializers.end()) {
    return it->second(value);
  }

  // Unknown attribute type - return nullptr
  return nullptr;
}

void AttributeFactory::registerDeserializer(const std::string & key, DeserializerFunc func)
{
  getDeserializers()[key] = std::move(func);
}

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
}

}  // namespace fibril_transport_common

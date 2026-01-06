#include "fibril_transport_common/attribute_system.hpp"

#include <cstring>
#include <map>
#include <stdexcept>

#include "binary.hpp"

namespace fibril_transport_common
{

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

}  // namespace fibril_transport_common

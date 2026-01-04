#include "fibril_transport_common/definition_binary.hpp"

namespace fibril
{

// ============================================================================
// DefinitionBinaryBuilder Implementation
// ============================================================================

DefinitionBinaryBuilder::DefinitionBinaryBuilder() {}

void DefinitionBinaryBuilder::begin(const std::string & node_name)
{
  node_name_ = node_name;
  ports_.clear();
  data_.clear();
}

void DefinitionBinaryBuilder::addPort(
  const std::string & port_name, PortDirection direction, const std::vector<uint8_t> & type_data,
  const std::vector<std::pair<std::string, std::string>> & metadata)
{
  std::vector<uint8_t> port_data;

  // Port name length + name
  port_data.push_back(static_cast<uint8_t>(port_name.size()));
  port_data.insert(port_data.end(), port_name.begin(), port_name.end());

  // Direction
  port_data.push_back(static_cast<uint8_t>(direction));

  // Type info length + type data
  uint16_t type_length = static_cast<uint16_t>(type_data.size());
  port_data.push_back(type_length & 0xFF);
  port_data.push_back((type_length >> 8) & 0xFF);
  port_data.insert(port_data.end(), type_data.begin(), type_data.end());

  // Metadata count
  port_data.push_back(static_cast<uint8_t>(metadata.size()));

  // Metadata entries
  for (const auto & [key, value] : metadata) {
    // Key length + key
    port_data.push_back(static_cast<uint8_t>(key.size()));
    port_data.insert(port_data.end(), key.begin(), key.end());

    // Value length + value
    uint16_t value_length = static_cast<uint16_t>(value.size());
    port_data.push_back(value_length & 0xFF);
    port_data.push_back((value_length >> 8) & 0xFF);
    port_data.insert(port_data.end(), value.begin(), value.end());
  }

  ports_.push_back(port_data);
}

std::vector<uint8_t> DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId type_id)
{
  std::vector<uint8_t> result;
  result.push_back(static_cast<uint8_t>(TypeKind::Primitive));
  result.push_back(static_cast<uint8_t>(type_id));
  return result;
}

std::vector<uint8_t> DefinitionBinaryBuilder::serializeStructType(
  const std::string & struct_name,
  const std::vector<std::tuple<std::string, PrimitiveTypeId, uint16_t>> & fields)
{
  std::vector<uint8_t> result;

  // Type kind (struct)
  result.push_back(static_cast<uint8_t>(TypeKind::Struct));

  // Struct name length + name
  result.push_back(static_cast<uint8_t>(struct_name.size()));
  result.insert(result.end(), struct_name.begin(), struct_name.end());

  // Field count
  result.push_back(static_cast<uint8_t>(fields.size()));

  // Each field
  for (const auto & [field_name, primitive_type, array_size] : fields) {
    // Field name length + name
    result.push_back(static_cast<uint8_t>(field_name.size()));
    result.insert(result.end(), field_name.begin(), field_name.end());

    // Primitive type
    result.push_back(static_cast<uint8_t>(primitive_type));

    // Array size (little-endian)
    result.push_back(array_size & 0xFF);
    result.push_back((array_size >> 8) & 0xFF);
  }

  return result;
}

std::vector<uint8_t> DefinitionBinaryBuilder::build()
{
  std::vector<uint8_t> result;

  // Header: Magic number (4 bytes, little-endian)
  writeUInt32(DEFINITION_MAGIC_NUMBER);

  // Version (2 bytes, little-endian)
  writeUInt16(DEFINITION_VERSION);

  // Node name length + name
  writeUInt8(static_cast<uint8_t>(node_name_.size()));
  writeString(node_name_);

  // Port count
  writeUInt8(static_cast<uint8_t>(ports_.size()));

  // Port entries
  for (const auto & port : ports_) {
    writeBytes(port);
  }

  return data_;
}

void DefinitionBinaryBuilder::writeUInt8(uint8_t value) { data_.push_back(value); }

void DefinitionBinaryBuilder::writeUInt16(uint16_t value)
{
  data_.push_back(value & 0xFF);
  data_.push_back((value >> 8) & 0xFF);
}

void DefinitionBinaryBuilder::writeUInt32(uint32_t value)
{
  data_.push_back(value & 0xFF);
  data_.push_back((value >> 8) & 0xFF);
  data_.push_back((value >> 16) & 0xFF);
  data_.push_back((value >> 24) & 0xFF);
}

void DefinitionBinaryBuilder::writeString(const std::string & str)
{
  data_.insert(data_.end(), str.begin(), str.end());
}

void DefinitionBinaryBuilder::writeBytes(const std::vector<uint8_t> & bytes)
{
  data_.insert(data_.end(), bytes.begin(), bytes.end());
}

// ============================================================================
// DefinitionBinaryReader Implementation
// ============================================================================

DefinitionBinaryReader::DefinitionBinaryReader(const std::vector<uint8_t> & data)
: data_(data), pos_(0)
{
}

bool DefinitionBinaryReader::parseHeader(std::string & node_name, uint8_t & port_count)
{
  // Check minimum size for header
  if (data_.size() < 7) {  // magic(4) + version(2) + name_len(1)
    return false;
  }

  // Read and validate magic number
  uint32_t magic = readUInt32();
  if (magic != DEFINITION_MAGIC_NUMBER) {
    return false;
  }

  // Read and validate version
  uint16_t version = readUInt16();
  if (version != DEFINITION_VERSION) {
    return false;
  }

  // Read node name
  uint8_t name_length = readUInt8();
  if (pos_ + name_length >= data_.size()) {
    return false;
  }
  node_name = readString(name_length);

  // Read port count
  if (pos_ >= data_.size()) {
    return false;
  }
  port_count = readUInt8();

  return true;
}

uint8_t DefinitionBinaryReader::readUInt8()
{
  if (pos_ >= data_.size()) {
    return 0;
  }
  return data_[pos_++];
}

uint16_t DefinitionBinaryReader::readUInt16()
{
  if (pos_ + 1 >= data_.size()) {
    return 0;
  }
  uint16_t value = data_[pos_] | (data_[pos_ + 1] << 8);
  pos_ += 2;
  return value;
}

uint32_t DefinitionBinaryReader::readUInt32()
{
  if (pos_ + 3 >= data_.size()) {
    return 0;
  }
  uint32_t value =
    data_[pos_] | (data_[pos_ + 1] << 8) | (data_[pos_ + 2] << 16) | (data_[pos_ + 3] << 24);
  pos_ += 4;
  return value;
}

std::string DefinitionBinaryReader::readString(uint8_t length)
{
  if (pos_ + length > data_.size()) {
    return "";
  }
  std::string result(data_.begin() + pos_, data_.begin() + pos_ + length);
  pos_ += length;
  return result;
}

std::vector<uint8_t> DefinitionBinaryReader::readBytes(size_t length)
{
  if (pos_ + length > data_.size()) {
    return {};
  }
  std::vector<uint8_t> result(data_.begin() + pos_, data_.begin() + pos_ + length);
  pos_ += length;
  return result;
}

}  // namespace fibril

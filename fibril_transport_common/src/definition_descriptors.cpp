#include "fibril_transport_common/definition_descriptors.hpp"

#include <cstring>
#include <stdexcept>

namespace fibril_transport_common
{
// ========================================
// CRC32 Implementation
// ========================================

namespace
{

// CRC32 table (generated once)
uint32_t crc32_table[256];
bool crc32_table_initialized = false;

void init_crc32_table()
{
  if (crc32_table_initialized) return;

  for (uint32_t i = 0; i < 256; i++) {
    uint32_t crc = i;
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
    crc32_table[i] = crc;
  }
  crc32_table_initialized = true;
}

uint32_t calculate_crc32(const std::vector<uint8_t> & data)
{
  init_crc32_table();

  uint32_t crc = 0xFFFFFFFF;
  for (uint8_t byte : data) {
    crc = (crc >> 8) ^ crc32_table[(crc ^ byte) & 0xFF];
  }
  return ~crc;
}

// Binary write helpers
void writeUInt8(std::vector<uint8_t> & buffer, uint8_t value) { buffer.push_back(value); }

void writeUInt16(std::vector<uint8_t> & buffer, uint16_t value)
{
  buffer.push_back(static_cast<uint8_t>(value & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void writeUInt32(std::vector<uint8_t> & buffer, uint32_t value)
{
  buffer.push_back(static_cast<uint8_t>(value & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void writeString(std::vector<uint8_t> & buffer, const std::string & str)
{
  buffer.insert(buffer.end(), str.begin(), str.end());
}

void writeBytes(std::vector<uint8_t> & buffer, const std::vector<uint8_t> & bytes)
{
  buffer.insert(buffer.end(), bytes.begin(), bytes.end());
}

// Binary read helpers
uint8_t readUInt8(const std::vector<uint8_t> & data, size_t & offset)
{
  if (offset >= data.size()) {
    throw std::runtime_error("Buffer underflow reading uint8");
  }
  return data[offset++];
}

uint16_t readUInt16(const std::vector<uint8_t> & data, size_t & offset)
{
  if (offset + 1 >= data.size()) {
    throw std::runtime_error("Buffer underflow reading uint16");
  }
  uint16_t value = data[offset] | (static_cast<uint16_t>(data[offset + 1]) << 8);
  offset += 2;
  return value;
}

uint32_t readUInt32(const std::vector<uint8_t> & data, size_t & offset)
{
  if (offset + 3 >= data.size()) {
    throw std::runtime_error("Buffer underflow reading uint32");
  }
  uint32_t value = data[offset] | (static_cast<uint32_t>(data[offset + 1]) << 8) |
                   (static_cast<uint32_t>(data[offset + 2]) << 16) |
                   (static_cast<uint32_t>(data[offset + 3]) << 24);
  offset += 4;
  return value;
}

std::string readString(const std::vector<uint8_t> & data, size_t & offset, size_t length)
{
  if (offset + length > data.size()) {
    throw std::runtime_error("Buffer underflow reading string");
  }
  std::string str(reinterpret_cast<const char *>(&data[offset]), length);
  offset += length;
  return str;
}

std::vector<uint8_t> readBytes(const std::vector<uint8_t> & data, size_t & offset, size_t length)
{
  if (offset + length > data.size()) {
    throw std::runtime_error("Buffer underflow reading bytes");
  }
  std::vector<uint8_t> bytes(data.begin() + offset, data.begin() + offset + length);
  offset += length;
  return bytes;
}

// Serialize TypeDescriptor
std::vector<uint8_t> serializeType(const TypeDescriptor & type, const NodeDescriptor & node);

// Serialize field for struct type
void serializeField(
  std::vector<uint8_t> & buffer, const FieldDescriptor & field, const NodeDescriptor & node)
{
  // Field name length and name
  writeUInt8(buffer, static_cast<uint8_t>(field.name.size()));
  writeString(buffer, field.name);

  // For now, only support primitive types or primitive arrays
  if (field.type.isPrimitive()) {
    // Primitive type
    writeUInt8(buffer, static_cast<uint8_t>(field.type.asPrimitive()));
    writeUInt16(buffer, 0);  // Array size = 0 (scalar)
  } else if (field.type.isArray() && field.type.arrayElement().isPrimitive()) {
    // Primitive array
    writeUInt8(buffer, static_cast<uint8_t>(field.type.arrayElement().asPrimitive()));
    writeUInt16(buffer, static_cast<uint16_t>(field.type.arraySize()));
  } else {
    throw std::runtime_error(
      "Only primitive types and primitive arrays are supported in binary format");
  }
}

// Serialize TypeDescriptor to TypeInfo format
std::vector<uint8_t> serializeType(const TypeDescriptor & type, const NodeDescriptor & node)
{
  std::vector<uint8_t> buffer;

  if (type.isPrimitive()) {
    // Type kind: Primitive
    writeUInt8(buffer, 0);
    // Primitive type ID
    writeUInt8(buffer, static_cast<uint8_t>(type.asPrimitive()));
  } else if (type.isStruct()) {
    // Type kind: Struct
    writeUInt8(buffer, 1);

    // Find struct definition
    const StructDescriptor * struct_def = node.findStruct(type.asStructName());
    if (!struct_def) {
      throw std::runtime_error("Struct not found: " + type.asStructName());
    }

    // Struct name
    writeUInt8(buffer, static_cast<uint8_t>(struct_def->name.size()));
    writeString(buffer, struct_def->name);

    // Field count
    writeUInt8(buffer, static_cast<uint8_t>(struct_def->fields.size()));

    // Fields
    for (const auto & field : struct_def->fields) {
      serializeField(buffer, field, node);
    }
  } else {
    throw std::runtime_error("Arrays are not supported as top-level port types in binary format");
  }

  return buffer;
}

// Deserialize TypeDescriptor
TypeDescriptor deserializeType(
  const std::vector<uint8_t> & data, size_t & offset, std::vector<StructDescriptor> & structs);

// Deserialize field
FieldDescriptor deserializeField(const std::vector<uint8_t> & data, size_t & offset)
{
  // Field name
  uint8_t name_len = readUInt8(data, offset);
  auto field_name = readString(data, offset, name_len);

  // Primitive type
  uint8_t prim_type = readUInt8(data, offset);
  uint16_t array_size = readUInt16(data, offset);

  auto type =
    array_size == 0
      ? TypeDescriptor::makePrimitive(static_cast<PrimitiveType>(prim_type))
      : TypeDescriptor::makeArray(
          TypeDescriptor::makePrimitive(static_cast<PrimitiveType>(prim_type)), array_size);

  return FieldDescriptor{.name = std::move(field_name), .type = std::move(type)};
}

// Deserialize TypeDescriptor from TypeInfo
TypeDescriptor deserializeType(
  const std::vector<uint8_t> & data, size_t & offset, std::vector<StructDescriptor> & structs)
{
  uint8_t type_kind = readUInt8(data, offset);

  if (type_kind == 0) {
    // Primitive
    uint8_t prim_type = readUInt8(data, offset);
    return TypeDescriptor::makePrimitive(static_cast<PrimitiveType>(prim_type));
  } else if (type_kind == 1) {
    // Struct
    StructDescriptor struct_def;

    // Struct name
    uint8_t name_len = readUInt8(data, offset);
    struct_def.name = readString(data, offset, name_len);

    // Field count
    uint8_t field_count = readUInt8(data, offset);

    // Fields
    for (uint8_t i = 0; i < field_count; ++i) {
      struct_def.fields.push_back(deserializeField(data, offset));
    }

    // Add to structs list
    std::string struct_name = struct_def.name;
    structs.push_back(std::move(struct_def));

    return TypeDescriptor::makeStruct(struct_name);
  } else {
    throw std::runtime_error("Unknown type kind");
  }
}

}  // anonymous namespace

// ========================================
// Port Helper Functions
// ========================================

const std::string & getPortName(const Port & port)
{
  return std::visit([](const auto & p) -> const std::string & { return p.name; }, port);
}

const AttributeList & getPortAttributes(const Port & port)
{
  return std::visit([](const auto & p) -> const AttributeList & { return p.attributes; }, port);
}

AttributeList & getPortAttributesMut(Port & port)
{
  return std::visit([](auto & p) -> AttributeList & { return p.attributes; }, port);
}

PortDirection getPortDirection(const Port & port)
{
  if (std::holds_alternative<SubPort>(port)) return PortDirection::Sub;
  if (std::holds_alternative<PubPort>(port)) return PortDirection::Pub;
  return PortDirection::Service;
}

// ========================================
// NodeDescriptor Implementation
// ========================================

const Port * NodeDescriptor::findPort(const std::string & name) const
{
  for (const auto & port : ports) {
    if (getPortName(port) == name) {
      return &port;
    }
  }
  return nullptr;
}

const StructDescriptor * NodeDescriptor::findStruct(const std::string & name) const
{
  for (const auto & struct_def : structs) {
    if (struct_def.name == name) {
      return &struct_def;
    }
  }
  return nullptr;
}

std::vector<uint8_t> NodeDescriptor::serializeToBinary() const
{
  std::vector<uint8_t> buffer;

  // Magic number: "FBRL"
  writeUInt32(buffer, 0x4642524C);

  // Version: 0x0200
  writeUInt16(buffer, 0x0200);

  // Node name
  writeUInt8(buffer, static_cast<uint8_t>(name.size()));
  writeString(buffer, name);

  // Port count
  writeUInt8(buffer, static_cast<uint8_t>(ports.size()));

  // Port entries
  for (const auto & port : ports) {
    std::visit(
      [&](const auto & p) {
        // Port name
        writeUInt8(buffer, static_cast<uint8_t>(p.name.size()));
        writeString(buffer, p.name);

        // Direction
        writeUInt8(buffer, static_cast<uint8_t>(getPortDirection(port)));

        // Type info
        std::vector<uint8_t> type_info;
        if constexpr (std::is_same_v<std::decay_t<decltype(p)>, ServicePort>) {
          type_info = serializeType(p.request_type, *this);
        } else {
          type_info = serializeType(p.data_type, *this);
        }

        writeUInt16(buffer, static_cast<uint16_t>(type_info.size()));
        writeBytes(buffer, type_info);

        if constexpr (std::is_same_v<std::decay_t<decltype(p)>, ServicePort>) {
          type_info = serializeType(p.response_type, *this);
          writeUInt16(buffer, static_cast<uint16_t>(type_info.size()));
          writeBytes(buffer, type_info);
        }

        // Metadata (attributes)
        std::vector<uint8_t> metadata = p.attributes.serializeToBinary();
        writeBytes(buffer, metadata);
      },
      port);
  }

  return buffer;
}

NodeDescriptor NodeDescriptor::deserializeFromBinary(const std::vector<uint8_t> & data)
{
  size_t offset = 0;
  NodeDescriptor node;

  // Magic number
  uint32_t magic = readUInt32(data, offset);
  if (magic != 0x4642524C) {
    throw std::runtime_error("Invalid magic number");
  }

  // Version
  uint16_t version = readUInt16(data, offset);
  if (version != 0x0200) {
    throw std::runtime_error("Unsupported version");
  }

  // Node name
  uint8_t name_len = readUInt8(data, offset);
  node.name = readString(data, offset, name_len);

  // Port count
  uint8_t port_count = readUInt8(data, offset);

  // Port entries
  for (uint8_t i = 0; i < port_count; ++i) {
    // Port name
    uint8_t port_name_len = readUInt8(data, offset);
    std::string port_name = readString(data, offset, port_name_len);

    // Direction
    uint8_t direction = readUInt8(data, offset);

    // Type info
    uint16_t type_info_len = readUInt16(data, offset);
    size_t type_offset = offset;
    TypeDescriptor first_data_type = deserializeType(data, offset, node.structs);

    // Metadata (attributes)
    AttributeList attrs = AttributeList::deserializeFromBinary(data, offset);

    // Create port based on direction
    if (direction == static_cast<uint8_t>(PortDirection::Sub)) {
      SubPort port{
        .name = std::move(port_name),
        .data_type = std::move(first_data_type),
        .attributes = std::move(attrs)};
      node.ports.push_back(std::move(port));
    } else if (direction == static_cast<uint8_t>(PortDirection::Pub)) {
      PubPort port{
        .name = std::move(port_name),
        .data_type = std::move(first_data_type),
        .attributes = std::move(attrs)};
      node.ports.push_back(std::move(port));
    } else if (direction == static_cast<uint8_t>(PortDirection::Service)) {
      uint16_t response_type_info_len = readUInt16(data, offset);
      TypeDescriptor response_data_type = deserializeType(data, offset, node.structs);
      ServicePort port{
        .name = std::move(port_name),
        .request_type = std::move(first_data_type),
        .response_type = std::move(response_data_type),
        .attributes = std::move(attrs)};
      node.ports.push_back(std::move(port));
    }
  }

  return node;
}

uint32_t NodeDescriptor::calculateChecksum() const
{
  auto binary = serializeToBinary();
  return calculate_crc32(binary);
}

}  // namespace fibril_transport_common

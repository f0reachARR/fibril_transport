#include "fibril_transport_common/definition_descriptors.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <optional>
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

// Serialize field for struct type
void serializeField(
  std::vector<uint8_t> & buffer, const FieldDescriptor & field,
  const std::vector<StructDescriptor> & structs)
{
  // Field name length and name
  writeUInt8(buffer, static_cast<uint8_t>(field.name.size()));
  writeString(buffer, field.name);

  // Determine the inner type (for arrays, this is the element type)
  const auto & inner_type = field.type.isArray() ? field.type.arrayElement() : field.type;

  // Type Kind: 0=primitive, 1=struct
  if (inner_type.isPrimitive()) {
    writeUInt8(buffer, 0);                                               // Type Kind = primitive
    writeUInt8(buffer, static_cast<uint8_t>(inner_type.asPrimitive()));  // Type ID
  } else if (inner_type.isStruct()) {
    writeUInt8(buffer, 1);  // Type Kind = struct
    // Find struct index
    auto it = std::find_if(
      structs.begin(), structs.end(),
      [inner_type](const StructDescriptor & s) { return s.name == inner_type.asStructName(); });
    if (it == structs.end()) {
      throw std::runtime_error("Struct not found: " + inner_type.asStructName());
    }
    writeUInt8(buffer, std::distance(structs.begin(), it));  // Struct Index
  } else {
    throw std::runtime_error("Unsupported field type");
  }

  // Array Size: 0=scalar, >0=fixed array
  writeUInt8(buffer, field.type.isArray() ? static_cast<uint8_t>(field.type.arraySize()) : 0);

  // Metadata (attributes)
  const auto metadata = field.attributes.serializeToBinary();
  writeBytes(buffer, metadata);
}

// Serialize port type in simple format (Type Kind + Type)
void serializePortType(
  std::vector<uint8_t> & buffer, const TypeDescriptor & type,
  const std::vector<StructDescriptor> & structs)
{
  if (type.isPrimitive()) {
    writeUInt8(buffer, 0);                                         // Type Kind = primitive
    writeUInt8(buffer, static_cast<uint8_t>(type.asPrimitive()));  // Type ID
  } else if (type.isStruct()) {
    writeUInt8(buffer, 1);  // Type Kind = struct
    // Find struct index
    auto it = std::find_if(structs.begin(), structs.end(), [type](const StructDescriptor & s) {
      return s.name == type.asStructName();
    });
    if (it == structs.end()) {
      throw std::runtime_error("Struct not found: " + type.asStructName());
    }
    writeUInt8(buffer, std::distance(structs.begin(), it));  // Struct Index
  } else {
    throw std::runtime_error("Arrays are not supported as top-level port types");
  }
}

// Deserialize field
FieldDescriptor deserializeField(
  const std::vector<uint8_t> & data, size_t & offset, const std::vector<StructDescriptor> & structs)
{
  // Field name
  uint8_t name_len = readUInt8(data, offset);
  auto field_name = readString(data, offset, name_len);

  // Type Kind: 0=primitive, 1=struct
  uint8_t type_kind = readUInt8(data, offset);

  // Type: Primitive Type ID or Struct Index
  uint8_t type_id = readUInt8(data, offset);

  // Array Size: 0=scalar, >0=fixed array
  uint8_t array_size = readUInt8(data, offset);

  // Metadata (attributes)
  AttributeList attrs = AttributeList::deserializeFromBinary(data, offset);

  if (type_kind != 0 && type_kind != 1) {
    throw std::runtime_error("Unknown type kind in field");
  }

  TypeDescriptor type = type_kind == 0
                          ? TypeDescriptor::makePrimitive(static_cast<PrimitiveType>(type_id))
                          : TypeDescriptor::makeStruct(structs.at(type_id).name);

  // Wrap in array if needed
  if (array_size > 0) {
    type = TypeDescriptor::makeArray(type, array_size);
  }

  return FieldDescriptor(field_name, type, std::move(attrs));
}

// Deserialize port type in simple format (Type Kind + Type)
TypeDescriptor deserializePortType(
  const std::vector<uint8_t> & data, size_t & offset, const std::vector<StructDescriptor> & structs)
{
  // Type Kind: 0=primitive, 1=struct
  uint8_t type_kind = readUInt8(data, offset);

  // Type: Primitive Type ID or Struct Index
  uint8_t type_id = readUInt8(data, offset);

  if (type_kind != 0 && type_kind != 1) {
    throw std::runtime_error("Unknown type kind in port");
  }

  TypeDescriptor type = type_kind == 0
                          ? TypeDescriptor::makePrimitive(static_cast<PrimitiveType>(type_id))
                          : TypeDescriptor::makeStruct(structs.at(type_id).name);

  return type;
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
  return std::visit(
    overload{
      [](const SubPort &) -> PortDirection { return PortDirection::Sub; },
      [](const PubPort &) -> PortDirection { return PortDirection::Pub; },
      [](const ServicePort &) -> PortDirection { return PortDirection::Service; },
    },
    port);
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

  // Struct count
  writeUInt8(buffer, static_cast<uint8_t>(structs.size()));

  // Port count
  writeUInt8(buffer, static_cast<uint8_t>(ports.size()));

  // Struct entries
  for (const auto & struct_def : structs) {
    writeUInt8(buffer, static_cast<uint8_t>(struct_def.name.size()));
    writeString(buffer, struct_def.name);

    // Field count
    writeUInt8(buffer, static_cast<uint8_t>(struct_def.fields.size()));

    // Fields
    for (const auto & field : struct_def.fields) {
      serializeField(buffer, field, structs);
    }

    // Attributes
    const auto metadata = struct_def.attributes.serializeToBinary();
    writeBytes(buffer, metadata);
  }

  // Port entries
  for (const auto & port : ports) {
    std::visit(
      [&](const auto & p) {
        // Port name
        writeUInt8(buffer, static_cast<uint8_t>(p.name.size()));
        writeString(buffer, p.name);

        // Direction
        writeUInt8(buffer, static_cast<uint8_t>(getPortDirection(port)));

        // 1st Type (Type Kind + Type)
        if constexpr (std::is_same_v<std::decay_t<decltype(p)>, ServicePort>) {
          serializePortType(buffer, p.request_type, structs);
          serializePortType(buffer, p.response_type, structs);
        } else {
          serializePortType(buffer, p.data_type, structs);
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

  // Struct count
  uint8_t struct_count = readUInt8(data, offset);

  // Port count
  uint8_t port_count = readUInt8(data, offset);

  // Struct entries
  for (uint8_t i = 0; i < struct_count; ++i) {
    // Struct name
    uint8_t struct_name_len = readUInt8(data, offset);
    StructDescriptor struct_def(readString(data, offset, struct_name_len));

    // Field count
    uint8_t field_count = readUInt8(data, offset);

    // Fields
    for (uint8_t j = 0; j < field_count; ++j) {
      struct_def.fields.push_back(deserializeField(data, offset, node.structs));
    }

    // Attributes
    struct_def.attributes = AttributeList::deserializeFromBinary(data, offset);

    node.structs.push_back(std::move(struct_def));
  }

  // Port entries
  for (uint8_t i = 0; i < port_count; ++i) {
    // Port name
    uint8_t port_name_len = readUInt8(data, offset);
    std::string port_name = readString(data, offset, port_name_len);

    // Direction
    PortDirection direction = static_cast<PortDirection>(readUInt8(data, offset));

    // 1st Type (Type Kind + Type)
    TypeDescriptor first_type = deserializePortType(data, offset, node.structs);

    // 2nd Type (for Service only)
    std::optional<TypeDescriptor> second_type;
    if (direction == PortDirection::Service) {
      second_type = deserializePortType(data, offset, node.structs);
    }

    // Metadata (attributes)
    AttributeList attrs = AttributeList::deserializeFromBinary(data, offset);

    // Create port based on direction
    if (direction == PortDirection::Sub) {
      SubPort port(port_name, first_type, std::move(attrs));
      node.ports.push_back(std::move(port));
    } else if (direction == PortDirection::Pub) {
      PubPort port(port_name, first_type, std::move(attrs));
      node.ports.push_back(std::move(port));
    } else if (direction == PortDirection::Service) {
      ServicePort port(port_name, first_type, *second_type, std::move(attrs));
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

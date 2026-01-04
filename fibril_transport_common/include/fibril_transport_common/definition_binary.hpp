#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fibril
{

// Magic number "FBRL" (0x4642524C)
constexpr uint32_t DEFINITION_MAGIC_NUMBER = 0x4642524C;

// Protocol version (0x0200 = v2.0)
constexpr uint16_t DEFINITION_VERSION = 0x0200;

/**
 * @brief Primitive type IDs as defined in the specification
 * 
 * These IDs correspond to the binary encoding defined in main.md Appendix B.
 */
enum class PrimitiveTypeId : uint8_t {
  Bool = 0x00,
  Int8 = 0x01,
  UInt8 = 0x02,
  Int16 = 0x03,
  UInt16 = 0x04,
  Int32 = 0x05,
  UInt32 = 0x06,
  Int64 = 0x07,
  UInt64 = 0x08,
  Float = 0x09,
  Double = 0x0A
};

/**
 * @brief Type kind discriminator
 */
enum class TypeKind : uint8_t { Primitive = 0, Struct = 1 };

/**
 * @brief Port direction
 */
enum class PortDirection : uint8_t {
  Sub = 0,   // Device subscribes (receives commands)
  Pub = 1,   // Device publishes (sends notifications)
  Param = 2  // Parameter (service-based)
};

// Pack all structures to ensure byte-aligned layout matching the specification
#pragma pack(push, 1)

/**
 * @brief Definition binary file header
 * 
 * Layout:
 * - Magic Number (4 bytes): "FBRL" (0x4642524C)
 * - Version (2 bytes): Protocol version (0x0200)
 * - Node Name Length (1 byte): N
 * - Node Name (N bytes): UTF-8 string
 * - Port Count (1 byte): P
 */
struct DefinitionBinaryHeader
{
  uint32_t magic_number;     // 0x4642524C ("FBRL")
  uint16_t version;          // 0x0200 (v2.0)
  uint8_t node_name_length;  // Length of node name
  // Followed by: node_name[node_name_length]
  // Followed by: uint8_t port_count
};

/**
 * @brief Field information within a struct type
 * 
 * Layout:
 * - Field Name Length (1 byte): L
 * - Field Name (L bytes): UTF-8 string
 * - Primitive Type (1 byte): Type ID
 * - Array Size (2 bytes): 0=scalar, >0=fixed array
 */
struct FieldInfo
{
  uint8_t field_name_length;
  // Followed by: field_name[field_name_length]
  // Followed by: uint8_t primitive_type
  // Followed by: uint16_t array_size
};

/**
 * @brief Type information (primitive or struct)
 * 
 * For Primitive:
 * - Type Kind (1 byte): 0
 * - Primitive Type (1 byte): Type ID
 * 
 * For Struct:
 * - Type Kind (1 byte): 1
 * - Struct Name Length (1 byte): S
 * - Struct Name (S bytes): UTF-8 string
 * - Field Count (1 byte): F
 * - Field Entries (F × FieldInfo)
 */
struct TypeInfoHeader
{
  uint8_t type_kind;  // TypeKind enum
  // For primitive: followed by uint8_t primitive_type
  // For struct: followed by struct_name_length, struct_name, field_count, fields...
};

/**
 * @brief Port entry in the definition
 * 
 * Layout:
 * - Port Name Length (1 byte): M
 * - Port Name (M bytes): UTF-8 string
 * - Direction (1 byte): 0=sub, 1=pub, 2=param
 * - Type Info Length (2 bytes): T
 * - Type Info (T bytes): Type definition
 * - Metadata Count (1 byte): K
 * - Metadata Entries (K × MetadataEntry)
 */
struct PortEntryHeader
{
  uint8_t port_name_length;
  // Followed by: port_name[port_name_length]
  // Followed by: uint8_t direction
  // Followed by: uint16_t type_info_length
  // Followed by: type_info[type_info_length]
  // Followed by: uint8_t metadata_count
  // Followed by: metadata_entries...
};

/**
 * @brief Metadata key-value entry
 * 
 * Layout:
 * - Key Length (1 byte): K
 * - Key (K bytes): UTF-8 string
 * - Value Length (2 bytes): V
 * - Value (V bytes): UTF-8 string or binary data
 */
struct MetadataEntry
{
  uint8_t key_length;
  // Followed by: key[key_length]
  // Followed by: uint16_t value_length
  // Followed by: value[value_length]
};

#pragma pack(pop)

/**
 * @brief Helper class for building definition binary data
 * 
 * This class provides utilities to construct a definition.bin from AST structures.
 */
class DefinitionBinaryBuilder
{
public:
  DefinitionBinaryBuilder();

  /**
   * @brief Start a new definition with node name
   */
  void begin(const std::string & node_name);

  /**
   * @brief Add a port to the definition
   * 
   * @param port_name Name of the port
   * @param direction Port direction (sub/pub/param)
   * @param type_data Serialized type information
   * @param metadata Key-value metadata pairs
   */
  void addPort(
    const std::string & port_name, PortDirection direction, const std::vector<uint8_t> & type_data,
    const std::vector<std::pair<std::string, std::string>> & metadata = {});

  /**
   * @brief Serialize a primitive type
   */
  static std::vector<uint8_t> serializePrimitiveType(PrimitiveTypeId type_id);

  /**
   * @brief Serialize a struct type
   * 
   * @param struct_name Name of the struct
   * @param fields Vector of (field_name, primitive_type, array_size) tuples
   */
  static std::vector<uint8_t> serializeStructType(
    const std::string & struct_name,
    const std::vector<std::tuple<std::string, PrimitiveTypeId, uint16_t>> & fields);

  /**
   * @brief Finalize and get the complete binary data
   */
  std::vector<uint8_t> build();

  /**
   * @brief Get current binary size
   */
  size_t size() const { return data_.size(); }

private:
  std::vector<uint8_t> data_;
  std::string node_name_;
  std::vector<std::vector<uint8_t>> ports_;

  void writeUInt8(uint8_t value);
  void writeUInt16(uint16_t value);
  void writeUInt32(uint32_t value);
  void writeString(const std::string & str);
  void writeBytes(const std::vector<uint8_t> & bytes);
};

/**
 * @brief Helper class for reading definition binary data
 * 
 * This class provides utilities to parse a definition.bin (for Master side).
 */
class DefinitionBinaryReader
{
public:
  explicit DefinitionBinaryReader(const std::vector<uint8_t> & data);

  /**
   * @brief Parse the header and validate magic number/version
   */
  bool parseHeader(std::string & node_name, uint8_t & port_count);

  /**
   * @brief Get the current read position
   */
  size_t position() const { return pos_; }

  /**
   * @brief Check if there's more data to read
   */
  bool hasMore() const { return pos_ < data_.size(); }

private:
  const std::vector<uint8_t> & data_;
  size_t pos_;

  uint8_t readUInt8();
  uint16_t readUInt16();
  uint32_t readUInt32();
  std::string readString(uint8_t length);
  std::vector<uint8_t> readBytes(size_t length);
};

}  // namespace fibril

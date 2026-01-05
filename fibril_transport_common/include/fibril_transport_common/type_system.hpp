#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace fibril_transport_common
{

/**
 * @brief Primitive type enumeration (matching binary format IDs)
 * 
 * These IDs correspond to the binary encoding defined in main.md Appendix B.
 */
enum class PrimitiveType : uint8_t {
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
 * @brief Get size in bytes for a primitive type
 * @param type Primitive type
 * @return Size in bytes
 */
size_t getPrimitiveTypeSize(PrimitiveType type);

/**
 * @brief Type descriptor (no location information)
 * 
 * Represents a type in the Fibril type system.
 * Can be primitive, struct reference, or array.
 * 
 * This class uses std::variant internally and is designed to be
 * copyable and movable efficiently.
 */
class TypeDescriptor
{
public:
  enum class Kind { Primitive, Struct, Array };

  // Factory methods
  static TypeDescriptor makePrimitive(PrimitiveType type);
  static TypeDescriptor makeStruct(std::string name);
  static TypeDescriptor makeArray(TypeDescriptor element, size_t size);

  // Copy/move constructors and assignment operators
  TypeDescriptor(const TypeDescriptor & other);
  TypeDescriptor(TypeDescriptor && other) noexcept;
  TypeDescriptor & operator=(const TypeDescriptor & other);
  TypeDescriptor & operator=(TypeDescriptor && other) noexcept;
  ~TypeDescriptor();

  // Kind query
  Kind kind() const;

  // Type checking helpers
  bool isPrimitive() const { return kind() == Kind::Primitive; }
  bool isStruct() const { return kind() == Kind::Struct; }
  bool isArray() const { return kind() == Kind::Array; }

  // Accessors (throw std::runtime_error if wrong kind)
  PrimitiveType asPrimitive() const;
  const std::string & asStructName() const;
  const TypeDescriptor & arrayElement() const;
  size_t arraySize() const;

private:
  struct PrimitiveData
  {
    PrimitiveType type;
  };

  struct StructData
  {
    std::string name;
  };

  struct ArrayData
  {
    std::unique_ptr<TypeDescriptor> element;
    size_t size;
  };

  std::variant<PrimitiveData, StructData, ArrayData> data_;

  // Private constructors for factory methods
  explicit TypeDescriptor(PrimitiveData data);
  explicit TypeDescriptor(StructData data);
  explicit TypeDescriptor(ArrayData data);
};

/**
 * @brief Field descriptor (no location information)
 * 
 * Represents a field within a struct.
 */
struct FieldDescriptor
{
  std::string name;
  TypeDescriptor type;
  std::optional<std::string> default_value;
};

/**
 * @brief Struct descriptor (no location information)
 * 
 * Represents a complete struct definition with all its fields.
 */
struct StructDescriptor
{
  std::string name;
  std::vector<FieldDescriptor> fields;

  /**
     * @brief Find a field by name
     * @param name Field name to search for
     * @return Pointer to field, or nullptr if not found
     */
  const FieldDescriptor * findField(const std::string & name) const;
};

}  // namespace fibril_transport_common

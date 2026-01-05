#include "fibril_transport_common/type_system.hpp"

#include <stdexcept>

namespace fibril_transport_common
{

// ========================================
// Primitive Type Utilities
// ========================================

size_t getPrimitiveTypeSize(PrimitiveType type)
{
  switch (type) {
    case PrimitiveType::Bool:
      return 1;
    case PrimitiveType::Int8:
      return 1;
    case PrimitiveType::UInt8:
      return 1;
    case PrimitiveType::Int16:
      return 2;
    case PrimitiveType::UInt16:
      return 2;
    case PrimitiveType::Int32:
      return 4;
    case PrimitiveType::UInt32:
      return 4;
    case PrimitiveType::Int64:
      return 8;
    case PrimitiveType::UInt64:
      return 8;
    case PrimitiveType::Float:
      return 4;
    case PrimitiveType::Double:
      return 8;
    default:
      throw std::runtime_error("Unknown primitive type");
  }
}

// ========================================
// TypeDescriptor Implementation
// ========================================

TypeDescriptor::TypeDescriptor(PrimitiveData data) : data_(std::move(data)) {}

TypeDescriptor::TypeDescriptor(StructData data) : data_(std::move(data)) {}

TypeDescriptor::TypeDescriptor(ArrayData data) : data_(std::move(data)) {}

TypeDescriptor TypeDescriptor::makePrimitive(PrimitiveType type)
{
  return TypeDescriptor(PrimitiveData{type});
}

TypeDescriptor TypeDescriptor::makeStruct(std::string name)
{
  return TypeDescriptor(StructData{std::move(name)});
}

TypeDescriptor TypeDescriptor::makeArray(TypeDescriptor element, size_t size)
{
  return TypeDescriptor(ArrayData{std::make_unique<TypeDescriptor>(std::move(element)), size});
}

// Copy constructor
TypeDescriptor::TypeDescriptor(const TypeDescriptor & other)
{
  if (std::holds_alternative<PrimitiveData>(other.data_)) {
    data_ = std::get<PrimitiveData>(other.data_);
  } else if (std::holds_alternative<StructData>(other.data_)) {
    data_ = std::get<StructData>(other.data_);
  } else if (std::holds_alternative<ArrayData>(other.data_)) {
    const auto & array_data = std::get<ArrayData>(other.data_);
    data_ = ArrayData{std::make_unique<TypeDescriptor>(*array_data.element), array_data.size};
  }
}

// Move constructor
TypeDescriptor::TypeDescriptor(TypeDescriptor && other) noexcept : data_(std::move(other.data_)) {}

// Copy assignment
TypeDescriptor & TypeDescriptor::operator=(const TypeDescriptor & other)
{
  if (this != &other) {
    if (std::holds_alternative<PrimitiveData>(other.data_)) {
      data_ = std::get<PrimitiveData>(other.data_);
    } else if (std::holds_alternative<StructData>(other.data_)) {
      data_ = std::get<StructData>(other.data_);
    } else if (std::holds_alternative<ArrayData>(other.data_)) {
      const auto & array_data = std::get<ArrayData>(other.data_);
      data_ = ArrayData{std::make_unique<TypeDescriptor>(*array_data.element), array_data.size};
    }
  }
  return *this;
}

// Move assignment
TypeDescriptor & TypeDescriptor::operator=(TypeDescriptor && other) noexcept
{
  if (this != &other) {
    data_ = std::move(other.data_);
  }
  return *this;
}

TypeDescriptor::~TypeDescriptor() = default;

TypeDescriptor::Kind TypeDescriptor::kind() const
{
  if (std::holds_alternative<PrimitiveData>(data_)) {
    return Kind::Primitive;
  } else if (std::holds_alternative<StructData>(data_)) {
    return Kind::Struct;
  } else {
    return Kind::Array;
  }
}

PrimitiveType TypeDescriptor::asPrimitive() const
{
  if (!isPrimitive()) {
    throw std::runtime_error("TypeDescriptor is not a primitive type");
  }
  return std::get<PrimitiveData>(data_).type;
}

const std::string & TypeDescriptor::asStructName() const
{
  if (!isStruct()) {
    throw std::runtime_error("TypeDescriptor is not a struct type");
  }
  return std::get<StructData>(data_).name;
}

const TypeDescriptor & TypeDescriptor::arrayElement() const
{
  if (!isArray()) {
    throw std::runtime_error("TypeDescriptor is not an array type");
  }
  return *std::get<ArrayData>(data_).element;
}

size_t TypeDescriptor::arraySize() const
{
  if (!isArray()) {
    throw std::runtime_error("TypeDescriptor is not an array type");
  }
  return std::get<ArrayData>(data_).size;
}

// ========================================
// StructDescriptor Implementation
// ========================================

const FieldDescriptor * StructDescriptor::findField(const std::string & name) const
{
  for (const auto & field : fields) {
    if (field.name == name) {
      return &field;
    }
  }
  return nullptr;
}

}  // namespace fibril_transport_common

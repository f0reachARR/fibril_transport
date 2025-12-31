#include "ast.hpp"

namespace fibril
{

// Type factory methods
Type Type::makePrimitive(PrimitiveType ptype, size_t line, size_t col)
{
  Type t;
  t.kind = Kind::Primitive;
  t.primitive_type = ptype;
  t.line = line;
  t.column = col;
  return t;
}

Type Type::makeStruct(const std::string & name, size_t line, size_t col)
{
  Type t;
  t.kind = Kind::Struct;
  t.struct_name = name;
  t.line = line;
  t.column = col;
  return t;
}

Type Type::makeArray(Type elem_type, size_t size, size_t line, size_t col)
{
  Type t;
  t.kind = Kind::Array;
  t.element_type = std::make_unique<Type>(std::move(elem_type));
  t.array_size = size;
  t.line = line;
  t.column = col;
  return t;
}

// StructDefinition
std::optional<std::string> StructDefinition::getRosType() const
{
  for (const auto & attr : attributes) {
    if (attr.name == "ros_type" && !attr.arguments.empty()) {
      return attr.arguments[0];
    }
  }
  return std::nullopt;
}

// SourceFile
const StructDefinition * SourceFile::findStruct(const std::string & name) const
{
  for (const auto & s : structs) {
    if (s.name == name) {
      return &s;
    }
  }
  return nullptr;
}

}  // namespace fibril

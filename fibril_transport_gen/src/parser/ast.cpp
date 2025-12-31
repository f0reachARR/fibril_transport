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

// AttributeValue factory methods
AttributeValue AttributeValue::makeString(const std::string & s)
{
  AttributeValue v;
  v.kind = Kind::String;
  v.value = s;
  return v;
}

AttributeValue AttributeValue::makeNumber(double n)
{
  AttributeValue v;
  v.kind = Kind::Number;
  v.value = n;
  return v;
}

AttributeValue AttributeValue::makeIdentifier(const std::string & id)
{
  AttributeValue v;
  v.kind = Kind::Identifier;
  v.value = id;
  return v;
}

std::string AttributeValue::asString() const
{
  if (std::holds_alternative<std::string>(value)) {
    return std::get<std::string>(value);
  }
  return "";
}

double AttributeValue::asNumber() const
{
  if (std::holds_alternative<double>(value)) {
    return std::get<double>(value);
  }
  return 0.0;
}

// Attribute helper methods
std::optional<std::string> Attribute::getStringArg(size_t index) const
{
  if (index < arguments.size()) {
    return arguments[index].asString();
  }
  return std::nullopt;
}

std::optional<double> Attribute::getNumberArg(size_t index) const
{
  if (index < arguments.size()) {
    if (arguments[index].kind == AttributeValue::Kind::Number) {
      return arguments[index].asNumber();
    }
  }
  return std::nullopt;
}

// StructDefinition
std::optional<std::string> StructDefinition::getRosType() const
{
  for (const auto & attr : attributes) {
    if (attr.name == "ros_type") {
      return attr.getStringArg(0);
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

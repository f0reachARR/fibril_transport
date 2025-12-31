#include "ast.hpp"

namespace fibril
{

// Type factory methods
Type Type::makePrimitive(PrimitiveType ptype, size_t line, size_t col)
{
  Type t;
  t.value = ptype;
  t.line = line;
  t.column = col;
  return t;
}

Type Type::makeStruct(const std::string & name, size_t line, size_t col)
{
  Type t;
  StructType st;
  st.name = name;
  st.line = line;
  st.column = col;
  t.value = st;
  t.line = line;
  t.column = col;
  return t;
}

Type Type::makeArray(Type elem_type, size_t size, size_t line, size_t col)
{
  Type t;
  ArrayType at;
  at.element_type = std::make_unique<Type>(std::move(elem_type));
  at.size = size;
  at.line = line;
  at.column = col;
  t.value = std::move(at);
  t.line = line;
  t.column = col;
  return t;
}

// Attribute helper methods
std::optional<std::string> Attribute::getStringArg(size_t index) const
{
  if (index < arguments.size()) {
    if (std::holds_alternative<std::string>(arguments[index])) {
      return std::get<std::string>(arguments[index]);
    }
    // 数値を文字列に変換
    if (std::holds_alternative<double>(arguments[index])) {
      return std::to_string(std::get<double>(arguments[index]));
    }
  }
  return std::nullopt;
}

std::optional<double> Attribute::getNumberArg(size_t index) const
{
  if (index < arguments.size()) {
    if (std::holds_alternative<double>(arguments[index])) {
      return std::get<double>(arguments[index]);
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

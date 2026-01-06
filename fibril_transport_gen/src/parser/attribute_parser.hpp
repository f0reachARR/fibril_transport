#pragma once

#include <tree_sitter/api.h>

#include <fibril_transport_common/attribute_system.hpp>
#include <variant>
#include <vector>

namespace fibril
{

struct AttributeParameter
{
  struct String
  {
    std::string str;
    String(std::string str) : str(str) {}
  };

  struct Integer
  {
    int value;
    Integer(int value) : value(value) {}
  };

  struct FloatingPoint
  {
    double value;
    FloatingPoint(double value) : value(value) {}
  };

  struct Identifier
  {
    std::string value;
    Identifier(std::string value) : value(value) {}
  };

  using Value = std::variant<String, Integer, FloatingPoint, Identifier>;

  std::vector<Value> values;
};

class AttributeParser
{
public:
  virtual std::string name() const = 0;

  virtual std::unique_ptr<fibril_transport_common::AttributeBase> parse(
    AttributeParameter & param) = 0;
};

class AttributeParserRegistry
{
public:
  static void registerParser(std::unique_ptr<AttributeParser> parser);

  static std::unique_ptr<fibril_transport_common::AttributeBase> parse(
    const std::string & name, AttributeParameter & param);

private:
  static std::map<std::string, std::unique_ptr<AttributeParser>> parsers_;
};

}  // namespace fibril

#include "attribute_parser.hpp"

namespace fibril
{
void AttributeParserRegistry::registerParser(std::unique_ptr<AttributeParser> parser)
{
  parsers_[parser->name()] = std::move(parser);
}

std::unique_ptr<fibril_transport_common::AttributeBase> AttributeParserRegistry::parse(
  const std::string & name, AttributeParameter & param)
{
  if (auto it = parsers_.find(name); it != parsers_.end()) {
    return it->second->parse(param);
  }
  return nullptr;
}

std::map<std::string, std::unique_ptr<AttributeParser>> AttributeParserRegistry::parsers_;
}  // namespace fibril
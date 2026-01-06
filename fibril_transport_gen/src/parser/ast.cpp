#include "ast.hpp"

namespace fibril
{

// SourceFile
const AstStructDescriptor * SourceFile::findStruct(const std::string & name) const
{
  for (const auto & s : structs) {
    if (s.name == name) {
      return &s;
    }
  }
  return nullptr;
}

}  // namespace fibril

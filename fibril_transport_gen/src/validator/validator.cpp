#include "validator.hpp"

#include <algorithm>
#include <sstream>

namespace fibril
{

// ValidationError
std::string ValidationError::format() const
{
  std::ostringstream oss;
  oss << file_path << ":" << line << ":" << column << ": " << message;
  return oss.str();
}

// TypeSizeCalculator
size_t TypeSizeCalculator::getPrimitiveSize(PrimitiveType type)
{
  switch (type) {
    case PrimitiveType::Bool:
    case PrimitiveType::Int8:
    case PrimitiveType::UInt8:
      return 1;
    case PrimitiveType::Int16:
    case PrimitiveType::UInt16:
      return 2;
    case PrimitiveType::Int32:
    case PrimitiveType::UInt32:
    case PrimitiveType::Float:
      return 4;
    case PrimitiveType::Int64:
    case PrimitiveType::UInt64:
    case PrimitiveType::Double:
      return 8;
    default:
      return 0;
  }
}

size_t TypeSizeCalculator::calculateSize(const Type & type, const SourceFile & source)
{
  switch (type.kind) {
    case Type::Kind::Primitive:
      return getPrimitiveSize(type.primitive_type);

    case Type::Kind::Array:
      if (type.element_type) {
        return calculateSize(*type.element_type, source) * type.array_size;
      }
      return 0;

    case Type::Kind::Struct: {
      // キャッシュチェック
      auto it = size_cache_.find(type.struct_name);
      if (it != size_cache_.end()) {
        return it->second;
      }

      // 構造体を検索
      const StructDefinition * struct_def = source.findStruct(type.struct_name);
      if (!struct_def) {
        return 0;
      }

      size_t size = calculateStructSize(*struct_def, source);
      size_cache_[type.struct_name] = size;
      return size;
    }

    default:
      return 0;
  }
}

size_t TypeSizeCalculator::calculateStructSize(
  const StructDefinition & struct_def, const SourceFile & source)
{
  size_t total_size = 0;
  size_t max_alignment = 1;

  for (const auto & field : struct_def.fields) {
    size_t field_size = calculateSize(field.type, source);
    size_t field_alignment = field_size;

    // アライメント調整
    if (total_size % field_alignment != 0) {
      total_size += field_alignment - (total_size % field_alignment);
    }

    total_size += field_size;
    max_alignment = std::max(max_alignment, field_alignment);
  }

  // 構造体全体のアライメント調整
  if (total_size % max_alignment != 0) {
    total_size += max_alignment - (total_size % max_alignment);
  }

  return total_size;
}

// Validator
Validator::Validator() {}

bool Validator::validate(const SourceFile & source)
{
  errors_.clear();
  warnings_.clear();
  defined_types_.clear();

  // フェーズ1: 型定義の収集
  collectTypeDefinitions(source);

  // フェーズ2: 重複定義チェック
  if (!checkDuplicateDefinitions(source)) {
    return false;
  }

  // フェーズ3: 型参照の解決
  if (!resolveTypeReferences(source)) {
    return false;
  }

  // フェーズ4: セマンティクスチェック
  if (!checkSemantics(source)) {
    return false;
  }

  return !hasErrors();
}

void Validator::collectTypeDefinitions(const SourceFile & source)
{
  for (const auto & struct_def : source.structs) {
    defined_types_.insert(struct_def.name);
  }
}

bool Validator::checkDuplicateDefinitions(const SourceFile & source)
{
  std::map<std::string, const StructDefinition *> struct_map;

  for (const auto & struct_def : source.structs) {
    auto it = struct_map.find(struct_def.name);
    if (it != struct_map.end()) {
      reportError(
        "Duplicate struct definition: '" + struct_def.name + "'", struct_def.line,
        struct_def.column, source.file_path);
      reportError(
        "Previous definition was here", it->second->line, it->second->column, source.file_path);
      return false;
    }
    struct_map[struct_def.name] = &struct_def;
  }

  // ノードの重複チェック
  std::map<std::string, const NodeDefinition *> node_map;
  for (const auto & node_def : source.nodes) {
    auto it = node_map.find(node_def.name);
    if (it != node_map.end()) {
      reportError(
        "Duplicate node definition: '" + node_def.name + "'", node_def.line, node_def.column,
        source.file_path);
      return false;
    }
    node_map[node_def.name] = &node_def;
  }

  return true;
}

bool Validator::resolveTypeReferences(const SourceFile & source)
{
  bool success = true;

  // 構造体のフィールド型を検証
  for (const auto & struct_def : source.structs) {
    for (const auto & field : struct_def.fields) {
      if (!resolveType(field.type, source)) {
        reportError(
          "Unknown type: '" + field.type.struct_name + "'", field.line, field.column,
          source.file_path);
        success = false;
      }
    }
  }

  // ノードのポート型を検証
  for (const auto & node_def : source.nodes) {
    for (const auto & port : node_def.ports) {
      if (!resolveType(port.data_type, source)) {
        reportError(
          "Unknown type in port '" + port.name + "': '" + port.data_type.struct_name + "'",
          port.line, port.column, source.file_path);
        success = false;
      }

      // サービスの要求/応答型
      if (port.request_type && !resolveType(*port.request_type, source)) {
        reportError(
          "Unknown request type in service '" + port.name + "'", port.line, port.column,
          source.file_path);
        success = false;
      }
      if (port.response_type && !resolveType(*port.response_type, source)) {
        reportError(
          "Unknown response type in service '" + port.name + "'", port.line, port.column,
          source.file_path);
        success = false;
      }
    }
  }

  return success;
}

bool Validator::resolveType(const Type & type, const SourceFile & source)
{
  switch (type.kind) {
    case Type::Kind::Primitive:
      return true;

    case Type::Kind::Array:
      if (type.element_type) {
        return resolveType(*type.element_type, source);
      }
      return false;

    case Type::Kind::Struct:
      return defined_types_.find(type.struct_name) != defined_types_.end();

    default:
      return false;
  }
}

bool Validator::checkSemantics(const SourceFile & source)
{
  bool success = true;

  // 構造体の検証
  for (const auto & struct_def : source.structs) {
    if (!validateStruct(struct_def, source)) {
      success = false;
    }
  }

  // ノードの検証
  for (const auto & node_def : source.nodes) {
    if (!validateNode(node_def, source)) {
      success = false;
    }
  }

  // 循環参照チェック
  for (const auto & struct_def : source.structs) {
    std::set<std::string> visiting;
    if (!checkCircularReference(struct_def.name, source, visiting)) {
      reportError(
        "Circular reference detected in struct: '" + struct_def.name + "'", struct_def.line,
        struct_def.column, source.file_path);
      success = false;
    }
  }

  return success;
}

bool Validator::validateStruct(const StructDefinition & struct_def, const SourceFile & source)
{
  if (struct_def.fields.empty()) {
    reportWarning(
      "Empty struct: '" + struct_def.name + "'", struct_def.line, struct_def.column,
      source.file_path);
  }

  // フィールド名の重複チェック
  std::set<std::string> field_names;
  for (const auto & field : struct_def.fields) {
    if (field_names.find(field.name) != field_names.end()) {
      reportError(
        "Duplicate field name: '" + field.name + "' in struct '" + struct_def.name + "'",
        field.line, field.column, source.file_path);
      return false;
    }
    field_names.insert(field.name);
  }

  return true;
}

bool Validator::validateNode(const NodeDefinition & node_def, const SourceFile & source)
{
  if (node_def.ports.empty()) {
    reportWarning(
      "Empty node: '" + node_def.name + "'", node_def.line, node_def.column, source.file_path);
  }

  // ポート名の重複チェック
  std::set<std::string> port_names;
  for (const auto & port : node_def.ports) {
    if (port_names.find(port.name) != port_names.end()) {
      reportError(
        "Duplicate port name: '" + port.name + "' in node '" + node_def.name + "'", port.line,
        port.column, source.file_path);
      return false;
    }
    port_names.insert(port.name);

    if (!validatePort(port, source)) {
      return false;
    }
  }

  return true;
}

bool Validator::validatePort(const Port & port, const SourceFile & source)
{
  // サービスポートの検証
  if (port.direction == Port::Direction::Service) {
    if (!port.request_type) {
      reportError(
        "Service port '" + port.name + "' missing request type", port.line, port.column,
        source.file_path);
      return false;
    }
    if (!port.response_type) {
      reportError(
        "Service port '" + port.name + "' missing response type", port.line, port.column,
        source.file_path);
      return false;
    }
  }

  return true;
}

bool Validator::checkCircularReference(
  const std::string & struct_name, const SourceFile & source, std::set<std::string> & visiting)
{
  // 既に訪問中なら循環参照
  if (visiting.find(struct_name) != visiting.end()) {
    return false;
  }

  const StructDefinition * struct_def = source.findStruct(struct_name);
  if (!struct_def) {
    return true;  // 未定義型はresolveTypeReferencesで検出済み
  }

  visiting.insert(struct_name);

  // 各フィールドをチェック
  for (const auto & field : struct_def->fields) {
    if (field.type.kind == Type::Kind::Struct) {
      if (!checkCircularReference(field.type.struct_name, source, visiting)) {
        return false;
      }
    } else if (field.type.kind == Type::Kind::Array && field.type.element_type) {
      if (field.type.element_type->kind == Type::Kind::Struct) {
        if (!checkCircularReference(field.type.element_type->struct_name, source, visiting)) {
          return false;
        }
      }
    }
  }

  visiting.erase(struct_name);
  return true;
}

void Validator::reportError(
  const std::string & message, size_t line, size_t column, const std::string & file)
{
  ValidationError err;
  err.message = message;
  err.line = line;
  err.column = column;
  err.file_path = file;
  errors_.push_back(err);
}

void Validator::reportWarning(
  const std::string & message, size_t line, size_t column, const std::string & file)
{
  ValidationError warn;
  warn.message = message;
  warn.line = line;
  warn.column = column;
  warn.file_path = file;
  warnings_.push_back(warn);
}

}  // namespace fibril

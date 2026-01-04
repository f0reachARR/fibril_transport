#include "validator.hpp"

#include <algorithm>
#include <sstream>

#include "ros_validator.hpp"

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
  return std::visit(
    [&](const auto & t) -> size_t {
      using T = std::decay_t<decltype(t)>;

      if constexpr (std::is_same_v<T, PrimitiveType>) {
        return getPrimitiveSize(t);
      } else if constexpr (std::is_same_v<T, StructType>) {
        // キャッシュチェック
        auto it = size_cache_.find(t.name);
        if (it != size_cache_.end()) {
          return it->second;
        }

        const StructDefinition * struct_def = source.findStruct(t.name);
        if (!struct_def) {
          return 0;
        }

        size_t size = calculateStructSize(*struct_def, source);
        size_cache_[t.name] = size;
        return size;
      } else if constexpr (std::is_same_v<T, ArrayType>) {
        if (t.element_type) {
          return calculateSize(*t.element_type, source) * t.size;
        }
        return 0;
      }

      return 0;
    },
    type.value);
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
Validator::Validator() : enable_ros_validation_(false) {}

Validator::~Validator() = default;

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

  // フェーズ5: ROS型検証（オプション）
  if (enable_ros_validation_ && ros_validator_) {
    if (!ros_validator_->validate(source, errors_)) {
      return false;
    }
  }

  return !hasErrors();
}

void Validator::collectTypeDefinitions(const SourceFile & source)
{
  for (const auto & struct_def : source.structs) {
    defined_types_.insert(struct_def.name);
  }
}

void Validator::setRosValidationEnabled(bool enabled)
{
  enable_ros_validation_ = enabled;

  if (enabled && !ros_validator_) {
    try {
      ros_validator_ = std::make_unique<RosValidator>();
    } catch (const std::exception & e) {
      // ROS validation initialization failed, disable it
      enable_ros_validation_ = false;
      // Note: Error will be reported when validation is attempted
      throw std::runtime_error(
        "Failed to enable ROS validation: " + std::string(e.what()) +
        "\nRun with --no-ros-validation to skip ROS type checking.");
    }
  } else if (!enabled) {
    ros_validator_.reset();
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
        std::string type_name = getTypeName(field.type);
        reportError(
          "Unknown type: '" + type_name + "'", field.line, field.column, source.file_path);
        success = false;
      }
    }
  }

  // ノードのポート型を検証
  for (const auto & node_def : source.nodes) {
    for (const auto & port : node_def.ports) {
      success &= std::visit(
        [&](const auto & p) -> bool {
          using T = std::decay_t<decltype(p)>;

          if constexpr (std::is_same_v<T, PubPort> || std::is_same_v<T, SubPort>) {
            if (!resolveType(p.data_type, source)) {
              std::string type_name = getTypeName(p.data_type);
              reportError(
                "Unknown type in port '" + p.name + "': '" + type_name + "'", p.line, p.column,
                source.file_path);
              return false;
            }
          } else if constexpr (std::is_same_v<T, ServicePort>) {
            if (!resolveType(p.request_type, source)) {
              std::string type_name = getTypeName(p.request_type);
              reportError(
                "Unknown request type in service '" + p.name + "': '" + type_name + "'", p.line,
                p.column, source.file_path);
              return false;
            }
            if (!resolveType(p.response_type, source)) {
              std::string type_name = getTypeName(p.response_type);
              reportError(
                "Unknown response type in service '" + p.name + "': '" + type_name + "'", p.line,
                p.column, source.file_path);
              return false;
            }
          }

          return true;
        },
        port);
    }
  }

  return success;
}

bool Validator::resolveType(const Type & type, const SourceFile & source)
{
  return std::visit(
    [&](const auto & t) -> bool {
      using T = std::decay_t<decltype(t)>;

      if constexpr (std::is_same_v<T, PrimitiveType>) {
        return true;
      } else if constexpr (std::is_same_v<T, StructType>) {
        return defined_types_.find(t.name) != defined_types_.end();
      } else if constexpr (std::is_same_v<T, ArrayType>) {
        if (t.element_type) {
          return resolveType(*t.element_type, source);
        }
        return false;
      }

      return false;
    },
    type.value);
}

std::string Validator::getTypeName(const Type & type)
{
  return std::visit(
    [](const auto & t) -> std::string {
      using T = std::decay_t<decltype(t)>;

      if constexpr (std::is_same_v<T, PrimitiveType>) {
        return "<primitive>";
      } else if constexpr (std::is_same_v<T, StructType>) {
        return t.name;
      } else if constexpr (std::is_same_v<T, ArrayType>) {
        return "<array>";
      }

      return "<unknown>";
    },
    type.value);
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
    std::string port_name = std::visit([](const auto & p) { return p.name; }, port);

    if (port_names.find(port_name) != port_names.end()) {
      auto [line, column] =
        std::visit([](const auto & p) { return std::make_pair(p.line, p.column); }, port);

      reportError(
        "Duplicate port name: '" + port_name + "' in node '" + node_def.name + "'", line, column,
        source.file_path);
      return false;
    }
    port_names.insert(port_name);
  }

  return true;
}

bool Validator::checkCircularReference(
  const std::string & struct_name, const SourceFile & source, std::set<std::string> & visiting)
{
  if (visiting.find(struct_name) != visiting.end()) {
    return false;
  }

  const StructDefinition * struct_def = source.findStruct(struct_name);
  if (!struct_def) {
    return true;
  }

  visiting.insert(struct_name);

  for (const auto & field : struct_def->fields) {
    bool has_circular = std::visit(
      [&](const auto & t) -> bool {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, StructType>) {
          return !checkCircularReference(t.name, source, visiting);
        } else if constexpr (std::is_same_v<T, ArrayType>) {
          if (t.element_type && t.element_type->isStruct()) {
            return !checkCircularReference(t.element_type->asStruct().name, source, visiting);
          }
        }

        return false;
      },
      field.type.value);

    if (has_circular) {
      return false;
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

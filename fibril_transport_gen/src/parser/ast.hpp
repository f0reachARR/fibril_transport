#pragma once

#include <fibril_transport_common/attribute_system.hpp>
#include <fibril_transport_common/definition_descriptors.hpp>
#include <fibril_transport_common/type_system.hpp>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace fibril
{

// Import common types
using fibril_transport_common::AttributeList;
using fibril_transport_common::FieldDescriptor;
using fibril_transport_common::Port;
using fibril_transport_common::PrimitiveType;
using fibril_transport_common::PubPort;
using fibril_transport_common::ServicePort;
using fibril_transport_common::StructDescriptor;
using fibril_transport_common::SubPort;
using fibril_transport_common::TypeDescriptor;

// ========================================
// AstNode Template (位置情報付きラッパー)
// ========================================

/**
 * @brief AST node wrapper that adds source location information
 * 
 * Wraps fibril_transport_common types with line/column information
 * for error reporting during parsing and validation.
 */
template <typename T>
struct AstNode : T
{
  size_t line;
  size_t column;

  AstNode() : line(0), column(0) {}

  AstNode(T val, size_t l = 0, size_t c = 0) : T(std::move(val)), line(l), column(c) {}

  // Implicit conversion to underlying type for easier usage
  operator const T &() const { return *this; }
  operator T &() { return *this; }

  const T & operator->() const { return *this; }
  T & operator->() { return *this; }

  const T & get() const { return *this; }
  T & get() { return *this; }
};

// ========================================
// DSL-specific declarations
// ========================================

// Syntax宣言
struct SyntaxDeclaration
{
  std::string version;
  size_t line;
  size_t column;
};

// Package宣言
struct PackageDeclaration
{
  std::string name;
  size_t line;
  size_t column;
};

// Import宣言
struct ImportDeclaration
{
  std::string path;
  size_t line;
  size_t column;
};

// Node定義
struct NodeDefinition
{
  std::string name;
  std::vector<AstNode<Port>> ports;
  size_t line;
  size_t column;
};

using AstStructDescriptor = AstNode<StructDescriptor>;
using AstFieldDescriptor = AstNode<FieldDescriptor>;
using AstPort = AstNode<Port>;

// ファイル全体のAST
struct SourceFile
{
  SyntaxDeclaration syntax;
  PackageDeclaration package;
  std::vector<ImportDeclaration> imports;
  std::vector<AstStructDescriptor> structs;
  std::vector<NodeDefinition> nodes;

  std::string file_path;

  // 名前からStructを検索
  const AstNode<StructDescriptor> * findStruct(const std::string & name) const;
};

}  // namespace fibril

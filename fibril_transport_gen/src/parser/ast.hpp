#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace fibril
{

// 基本型
enum class PrimitiveType {
  Bool,
  Int8,
  UInt8,
  Int16,
  UInt16,
  Int32,
  UInt32,
  Int64,
  UInt64,
  Float,
  Double
};

// 前方宣言
struct Type;

// 構造体型
struct StructType
{
  std::string name;
  size_t line;
  size_t column;
};

// 配列型
struct ArrayType
{
  std::unique_ptr<Type> element_type;
  size_t size;
  size_t line;
  size_t column;
};

// 型指定（variant版）
struct Type
{
  std::variant<PrimitiveType, StructType, ArrayType> value;
  size_t line;
  size_t column;

  // ファクトリーメソッド
  static Type makePrimitive(PrimitiveType ptype, size_t line = 0, size_t col = 0);
  static Type makeStruct(const std::string & name, size_t line = 0, size_t col = 0);
  static Type makeArray(Type elem_type, size_t size, size_t line = 0, size_t col = 0);

  // 型チェックヘルパー
  bool isPrimitive() const { return std::holds_alternative<PrimitiveType>(value); }
  bool isStruct() const { return std::holds_alternative<StructType>(value); }
  bool isArray() const { return std::holds_alternative<ArrayType>(value); }

  // アクセサ
  const PrimitiveType & asPrimitive() const { return std::get<PrimitiveType>(value); }
  const StructType & asStruct() const { return std::get<StructType>(value); }
  const ArrayType & asArray() const { return std::get<ArrayType>(value); }
};

// 属性の引数値（variant版 - kindフィールド不要）
using AttributeValue = std::variant<std::string, double>;

// 属性
struct Attribute
{
  std::string name;
  std::vector<AttributeValue> arguments;
  size_t line;
  size_t column;

  // ヘルパー：引数を文字列として取得
  std::optional<std::string> getStringArg(size_t index = 0) const;

  // ヘルパー：引数を数値として取得
  std::optional<double> getNumberArg(size_t index = 0) const;
};

// フィールド宣言（struct内）
struct FieldDeclaration
{
  std::vector<Attribute> attributes;
  Type type;
  std::string name;
  std::optional<std::string> default_value;
  size_t line;
  size_t column;
};

// Struct定義
struct StructDefinition
{
  std::vector<Attribute> attributes;
  std::string name;
  std::vector<FieldDeclaration> fields;
  size_t line;
  size_t column;

  // ros_type属性を取得
  std::optional<std::string> getRosType() const;
};

// Pub Port
struct PubPort
{
  std::vector<Attribute> attributes;
  std::string name;
  Type data_type;
  size_t line;
  size_t column;
};

// Sub Port
struct SubPort
{
  std::vector<Attribute> attributes;
  std::string name;
  Type data_type;
  size_t line;
  size_t column;
};

// Service Port
struct ServicePort
{
  std::vector<Attribute> attributes;
  std::string name;
  Type request_type;
  Type response_type;
  size_t line;
  size_t column;
};

// Port定義（variant版 - Directionフィールド不要）
using Port = std::variant<PubPort, SubPort, ServicePort>;

// Node定義
struct NodeDefinition
{
  std::string name;
  std::vector<Port> ports;
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

// Package宣言
struct PackageDeclaration
{
  std::string name;
  size_t line;
  size_t column;
};

// Syntax宣言
struct SyntaxDeclaration
{
  std::string version;
  size_t line;
  size_t column;
};

// ファイル全体のAST
struct SourceFile
{
  SyntaxDeclaration syntax;
  PackageDeclaration package;
  std::vector<ImportDeclaration> imports;
  std::vector<StructDefinition> structs;
  std::vector<NodeDefinition> nodes;

  std::string file_path;

  // 名前からStructを検索
  const StructDefinition * findStruct(const std::string & name) const;
};

}  // namespace fibril

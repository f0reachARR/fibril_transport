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

// 型指定
struct Type
{
  enum class Kind { Primitive, Struct, Array } kind;

  // Primitive型の場合
  PrimitiveType primitive_type;

  // Struct型の場合
  std::string struct_name;

  // Array型の場合
  std::unique_ptr<Type> element_type;
  size_t array_size;

  // ソース位置情報
  size_t line;
  size_t column;

  static Type makePrimitive(PrimitiveType ptype, size_t line = 0, size_t col = 0);
  static Type makeStruct(const std::string & name, size_t line = 0, size_t col = 0);
  static Type makeArray(Type elem_type, size_t size, size_t line = 0, size_t col = 0);
};

// 属性の引数値
struct AttributeValue
{
  enum class Kind { String, Number, Identifier } kind;

  std::variant<std::string, double> value;

  static AttributeValue makeString(const std::string & s);
  static AttributeValue makeNumber(double n);
  static AttributeValue makeIdentifier(const std::string & id);

  std::string asString() const;
  double asNumber() const;
};

// 属性
struct Attribute
{
  std::string name;
  std::vector<AttributeValue> arguments;
  size_t line;
  size_t column;

  // ヘルパー：最初の引数を文字列として取得
  std::optional<std::string> getStringArg(size_t index = 0) const;

  // ヘルパー：最初の引数を数値として取得
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

// Port定義
struct Port
{
  enum class Direction { Pub, Sub, Service } direction;

  std::vector<Attribute> attributes;
  std::string name;
  Type data_type;

  // Serviceの場合
  std::optional<Type> request_type;
  std::optional<Type> response_type;

  size_t line;
  size_t column;
};

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

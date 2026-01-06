#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "parser/ast.hpp"
#include "ros_validator.hpp"

namespace fibril
{

// バリデーションエラー
struct ValidationError
{
  std::string message;
  size_t line;
  size_t column;
  std::string file_path;

  std::string format() const;
};

// 型サイズ計算
class TypeSizeCalculator
{
public:
  // プリミティブ型のサイズ（バイト）
  static size_t getPrimitiveSize(PrimitiveType type);

  // 型のサイズ計算（再帰的）
  size_t calculateSize(const TypeDescriptor & type, const SourceFile & source);

  // 構造体のサイズ計算（アライメント考慮）
  size_t calculateStructSize(const StructDescriptor & struct_def, const SourceFile & source);

private:
  std::map<std::string, size_t> size_cache_;
};

// セマンティクス解析とバリデーション
class Validator
{
public:
  Validator();
  ~Validator();  // Destructor needed for unique_ptr with forward-declared type

  // ソースファイル全体の検証
  bool validate(const SourceFile & source);

  // ROS型検証の有効化/無効化
  void setRosValidationEnabled(bool enabled);

  // エラー取得
  const std::vector<ValidationError> & getErrors() const { return errors_; }
  const std::vector<ValidationError> & getWarnings() const { return warnings_; }

  bool hasErrors() const { return !errors_.empty(); }

private:
  std::vector<ValidationError> errors_;
  std::vector<ValidationError> warnings_;
  TypeSizeCalculator size_calc_;

  // ROS型検証（オプション）
  std::unique_ptr<RosValidator> ros_validator_;
  bool enable_ros_validation_;

  // フェーズ1: 型定義の収集
  void collectTypeDefinitions(const SourceFile & source);

  // フェーズ2: 型参照の解決
  bool resolveTypeReferences(const SourceFile & source);

  // フェーズ3: セマンティクスチェック
  bool checkSemantics(const SourceFile & source);

  // 型解決
  bool resolveType(const TypeDescriptor & type, const SourceFile & source);

  // 型名取得ヘルパー
  std::string getTypeName(const TypeDescriptor & type);

  // 構造体の検証
  bool validateStruct(const AstStructDescriptor & struct_def, const SourceFile & source);

  // ノードの検証
  bool validateNode(const NodeDefinition & node_def, const SourceFile & source);

  // 循環参照チェック
  bool checkCircularReference(
    const std::string & struct_name, const SourceFile & source, std::set<std::string> & visiting);

  // 重複定義チェック
  bool checkDuplicateDefinitions(const SourceFile & source);

  // エラー報告
  void reportError(
    const std::string & message, size_t line, size_t column, const std::string & file);
  void reportWarning(
    const std::string & message, size_t line, size_t column, const std::string & file);

  // 定義済み型の記録
  std::set<std::string> defined_types_;
};

}  // namespace fibril

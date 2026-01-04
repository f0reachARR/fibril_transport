#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "parser/ast.hpp"
#include "validator/validator.hpp"

namespace fibril
{

// コード生成の設定
struct CodeGenOptions
{
  bool generate_comments = true;
  bool generate_memory_estimates = true;
  size_t can_frame_size = 64;  // CAN-FDフレームサイズ
};

// 生成されたコード
struct GeneratedCode
{
  std::string header_content;
  std::string node_name;
  size_t data_size;
  size_t max_rx_mappings;
  size_t max_tx_mappings;
};

// C++コードジェネレータ
class CppGenerator
{
public:
  explicit CppGenerator(const CodeGenOptions & options = CodeGenOptions());

  // ノードからC++ヘッダを生成
  GeneratedCode generate(const NodeDefinition & node, const SourceFile & source);

private:
  CodeGenOptions options_;
  TypeSizeCalculator size_calc_;

  // ヘッダー生成の各セクション
  std::string generateHeaderGuard(const std::string & node_name, bool opening);
  std::string generateIncludes();
  std::string generateDataStruct(const NodeDefinition & node, const SourceFile & source);
  std::string generateIDMappingStruct();
  std::string generateServiceEnum(const NodeDefinition & node);
  std::string generateServiceStructs(const NodeDefinition & node, const SourceFile & source);
  std::string generateNodeClass(const NodeDefinition & node, const SourceFile & source);

  // ヘルパーメソッド
  std::string toCppType(const Type & type);
  std::string toSnakeCase(const std::string & str);
  std::string toUpperSnakeCase(const std::string & str);
  std::string toPascalCase(const std::string & str);

  // リソース計算
  size_t calculateMaxMappings(
    const std::vector<Port> & ports, bool is_rx, const SourceFile & source);
  size_t calculateDataSize(const NodeDefinition & node, const SourceFile & source);

  // ポート種別判定
  bool isPubPort(const Port & port);
  bool isSubPort(const Port & port);
  bool isServicePort(const Port & port);

  // ポート情報取得
  std::string getPortName(const Port & port);
  const Type & getPortDataType(const Port & port);
};

}  // namespace fibril

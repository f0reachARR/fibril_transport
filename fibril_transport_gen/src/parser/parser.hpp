#pragma once

#include <tree_sitter/api.h>

#include <filesystem>

#include "ast.hpp"
#include "node_cursor.hpp"

namespace fibril
{

class Parser
{
public:
  Parser();
  ~Parser();

  // 単一ファイルのパース
  SourceFile parse(const std::string & file_path);

  // 文字列から直接パース（LSP開発などに有用）
  SourceFile parseFromString(
    const std::string & source, const std::string & virtual_path = "<string>");

  // エラーメッセージ
  const std::vector<std::string> & getErrors() const { return errors_; }

private:
  TSParser * parser_;
  TSLanguage * language_;
  std::vector<std::string> errors_;

  // インポート解決済みファイルのキャッシュ
  std::map<std::string, SourceFile> parsed_files_;

  // CST -> AST変換
  SourceFile parseTree(TSTree * tree, const std::string & source, const std::string & file_path);

  // 各ノードタイプのパース
  SyntaxDeclaration parseSyntaxDeclaration(TSNode node, const std::string & source);
  PackageDeclaration parsePackageDeclaration(TSNode node, const std::string & source);
  ImportDeclaration parseImportDeclaration(TSNode node, const std::string & source);
  AstStructDescriptor parseStructDefinition(TSNode node, const std::string & source);
  NodeDefinition parseNodeDefinition(TSNode node, const std::string & source);
  AstFieldDescriptor parseFieldDeclaration(TSNode node, const std::string & source);
  AstPort parsePort(TSNode node, const std::string & source);
  TypeDescriptor parseType(TSNode node, const std::string & source);
  void parseAttributeInto(TSNode node, const std::string & source, AttributeList & attr_list);

  // ヘルパー関数
  std::string getNodeText(TSNode node, const std::string & source);
  std::string unquoteString(const std::string & quoted);
  PrimitiveType parsePrimitiveType(const std::string & type_name);

  // インポート解決
  std::string resolveImportPath(const std::string & import_path, const std::string & current_file);

  // Tree-sitter error detection
  bool hasTreeSitterErrors(TSNode node);

  // エラー報告
  void reportError(const std::string & message, size_t line, size_t column);
};

// Tree-sitter言語定義（外部リンク）
extern "C" {
TSLanguage * tree_sitter_fibril();
}

}  // namespace fibril

#include "parser.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace fibril
{

Parser::Parser()
{
  parser_ = ts_parser_new();
  language_ = tree_sitter_fibril();
  ts_parser_set_language(parser_, language_);
}

Parser::~Parser()
{
  if (parser_) {
    ts_parser_delete(parser_);
  }
}

SourceFile Parser::parse(const std::string & file_path)
{
  errors_.clear();

  // ファイルを読み込み
  std::ifstream file(file_path);
  if (!file.is_open()) {
    reportError("Failed to open file: " + file_path, 0, 0);
    return SourceFile{};
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string source = buffer.str();

  return parseFromString(source, file_path);
}

SourceFile Parser::parseFromString(const std::string & source, const std::string & virtual_path)
{
  errors_.clear();

  // Tree-sitterでパース
  TSTree * tree = ts_parser_parse_string(parser_, nullptr, source.c_str(), source.length());

  if (!tree) {
    reportError("Failed to parse source", 0, 0);
    return SourceFile{};
  }

  SourceFile result = parseTree(tree, source, virtual_path);
  ts_tree_delete(tree);

  return result;
}

SourceFile Parser::parseTree(
  TSTree * tree, const std::string & source, const std::string & file_path)
{
  SourceFile sf;
  sf.file_path = file_path;

  TSNode root = ts_tree_root_node(tree);

  // source_fileノードの子をイテレート
  uint32_t child_count = ts_node_child_count(root);
  for (uint32_t i = 0; i < child_count; ++i) {
    TSNode child = ts_node_child(root, i);
    std::string type = ts_node_type(child);

    if (type == "syntax_declaration") {
      sf.syntax = parseSyntaxDeclaration(child, source);
    } else if (type == "package_declaration") {
      sf.package = parsePackageDeclaration(child, source);
    } else if (type == "import_declaration") {
      sf.imports.push_back(parseImportDeclaration(child, source));
    } else if (type == "struct_definition") {
      sf.structs.push_back(parseStructDefinition(child, source));
    } else if (type == "node_definition") {
      sf.nodes.push_back(parseNodeDefinition(child, source));
    }
  }

  return sf;
}

SyntaxDeclaration Parser::parseSyntaxDeclaration(TSNode node, const std::string & source)
{
  SyntaxDeclaration decl;
  decl.line = ts_node_start_point(node).row + 1;
  decl.column = ts_node_start_point(node).column + 1;

  // string_literalを探す
  uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    if (std::string(ts_node_type(child)) == "string_literal") {
      decl.version = unquoteString(getNodeText(child, source));
      break;
    }
  }

  return decl;
}

PackageDeclaration Parser::parsePackageDeclaration(TSNode node, const std::string & source)
{
  PackageDeclaration decl;
  decl.line = ts_node_start_point(node).row + 1;
  decl.column = ts_node_start_point(node).column + 1;

  // qualified_identifierを探す
  uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    if (std::string(ts_node_type(child)) == "qualified_identifier") {
      decl.name = getNodeText(child, source);
      break;
    }
  }

  return decl;
}

ImportDeclaration Parser::parseImportDeclaration(TSNode node, const std::string & source)
{
  ImportDeclaration decl;
  decl.line = ts_node_start_point(node).row + 1;
  decl.column = ts_node_start_point(node).column + 1;

  // string_literalを探す
  uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    if (std::string(ts_node_type(child)) == "string_literal") {
      decl.path = unquoteString(getNodeText(child, source));
      break;
    }
  }

  return decl;
}

StructDefinition Parser::parseStructDefinition(TSNode node, const std::string & source)
{
  StructDefinition def;
  def.line = ts_node_start_point(node).row + 1;
  def.column = ts_node_start_point(node).column + 1;

  uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    std::string type = ts_node_type(child);

    if (type == "attribute") {
      def.attributes.push_back(parseAttribute(child, source));
    } else if (type == "identifier") {
      def.name = getNodeText(child, source);
    } else if (type == "field_declaration") {
      def.fields.push_back(parseFieldDeclaration(child, source));
    }
  }

  return def;
}

NodeDefinition Parser::parseNodeDefinition(TSNode node, const std::string & source)
{
  NodeDefinition def;
  def.line = ts_node_start_point(node).row + 1;
  def.column = ts_node_start_point(node).column + 1;

  uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    std::string type = ts_node_type(child);

    if (type == "identifier") {
      def.name = getNodeText(child, source);
    } else if (type == "port_declaration") {
      // port_declarationの中身を取得
      TSNode port_child = ts_node_child(child, 0);
      def.ports.push_back(parsePort(port_child, source));
    }
  }

  return def;
}

FieldDeclaration Parser::parseFieldDeclaration(TSNode node, const std::string & source)
{
  FieldDeclaration field;
  field.line = ts_node_start_point(node).row + 1;
  field.column = ts_node_start_point(node).column + 1;

  uint32_t count = ts_node_child_count(node);
  bool has_type = false;

  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    std::string type = ts_node_type(child);

    if (type == "attribute") {
      field.attributes.push_back(parseAttribute(child, source));
    } else if (type == "type_spec" && !has_type) {
      field.type = parseType(child, source);
      has_type = true;
    } else if (type == "identifier" && has_type) {
      field.name = getNodeText(child, source);
    } else if (type == "array_spec") {
      // 配列サイズを取得してtypeを更新
      TSNode size_node = ts_node_child(child, 1);  // '[' size ']'
      std::string size_str = getNodeText(size_node, source);
      size_t size = std::stoul(size_str);
      field.type = Type::makeArray(std::move(field.type), size, field.line, field.column);
    }
  }

  return field;
}

Port Parser::parsePort(TSNode node, const std::string & source)
{
  Port port;
  port.line = ts_node_start_point(node).row + 1;
  port.column = ts_node_start_point(node).column + 1;

  std::string port_type = ts_node_type(node);
  if (port_type == "pub_port") {
    port.direction = Port::Direction::Pub;
  } else if (port_type == "sub_port") {
    port.direction = Port::Direction::Sub;
  } else if (port_type == "service_port") {
    port.direction = Port::Direction::Service;
  }

  uint32_t count = ts_node_child_count(node);
  int type_count = 0;

  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    std::string type = ts_node_type(child);

    if (type == "attribute") {
      port.attributes.push_back(parseAttribute(child, source));
    } else if (type == "identifier") {
      port.name = getNodeText(child, source);
    } else if (type == "type_spec") {
      if (port.direction == Port::Direction::Service) {
        if (type_count == 0) {
          port.request_type = parseType(child, source);
        } else {
          port.response_type = parseType(child, source);
        }
        type_count++;
      } else {
        port.data_type = parseType(child, source);
      }
    }
  }

  return port;
}

Type Parser::parseType(TSNode node, const std::string & source)
{
  Type type;
  type.line = ts_node_start_point(node).row + 1;
  type.column = ts_node_start_point(node).column + 1;

  TSNode child = ts_node_child(node, 0);
  std::string child_type = ts_node_type(child);

  if (child_type == "primitive_type") {
    std::string type_name = getNodeText(child, source);
    type = Type::makePrimitive(parsePrimitiveType(type_name), type.line, type.column);
  } else if (child_type == "qualified_identifier") {
    std::string name = getNodeText(child, source);
    type = Type::makeStruct(name, type.line, type.column);
  }

  return type;
}

Attribute Parser::parseAttribute(TSNode node, const std::string & source)
{
  Attribute attr;
  attr.line = ts_node_start_point(node).row + 1;
  attr.column = ts_node_start_point(node).column + 1;

  uint32_t count = ts_node_child_count(node);
  bool found_name = false;

  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    std::string type = ts_node_type(child);

    if (type == "identifier" && !found_name) {
      attr.name = getNodeText(child, source);
      found_name = true;
    } else if (type == "attribute_item") {
      // attribute_itemの中身を解析
      TSNode item_id = ts_node_child(child, 0);
      if (ts_node_child_count(item_id) > 0 && std::string(ts_node_type(item_id)) == "identifier") {
        if (attr.name.empty()) {
          attr.name = getNodeText(item_id, source);
          found_name = true;
        }
      }
      // 引数を探す
      uint32_t item_count = ts_node_child_count(child);
      for (uint32_t j = 0; j < item_count; ++j) {
        TSNode arg = ts_node_child(child, j);
        std::string arg_type = ts_node_type(arg);
        if (arg_type == "string_literal") {
          attr.arguments.push_back(
            AttributeValue::makeString(unquoteString(getNodeText(arg, source))));
        } else if (arg_type == "number_literal") {
          std::string num_str = getNodeText(arg, source);
          attr.arguments.push_back(AttributeValue::makeNumber(std::stod(num_str)));
        } else if (arg_type == "qualified_identifier") {
          attr.arguments.push_back(AttributeValue::makeIdentifier(getNodeText(arg, source)));
        }
      }
    } else if (type == "string_literal") {
      attr.arguments.push_back(
        AttributeValue::makeString(unquoteString(getNodeText(child, source))));
    } else if (type == "number_literal") {
      std::string num_str = getNodeText(child, source);
      attr.arguments.push_back(AttributeValue::makeNumber(std::stod(num_str)));
    } else if (type == "qualified_identifier") {
      attr.arguments.push_back(AttributeValue::makeIdentifier(getNodeText(child, source)));
    }
  }

  return attr;
}

// ヘルパー関数
std::string Parser::getNodeText(TSNode node, const std::string & source)
{
  uint32_t start = ts_node_start_byte(node);
  uint32_t end = ts_node_end_byte(node);
  return source.substr(start, end - start);
}

std::string Parser::unquoteString(const std::string & quoted)
{
  if (quoted.length() >= 2 && quoted.front() == '"' && quoted.back() == '"') {
    return quoted.substr(1, quoted.length() - 2);
  }
  return quoted;
}

PrimitiveType Parser::parsePrimitiveType(const std::string & type_name)
{
  if (type_name == "bool") return PrimitiveType::Bool;
  if (type_name == "int8") return PrimitiveType::Int8;
  if (type_name == "uint8") return PrimitiveType::UInt8;
  if (type_name == "int16") return PrimitiveType::Int16;
  if (type_name == "uint16") return PrimitiveType::UInt16;
  if (type_name == "int32") return PrimitiveType::Int32;
  if (type_name == "uint32") return PrimitiveType::UInt32;
  if (type_name == "int64") return PrimitiveType::Int64;
  if (type_name == "uint64") return PrimitiveType::UInt64;
  if (type_name == "float") return PrimitiveType::Float;
  if (type_name == "double") return PrimitiveType::Double;

  // デフォルト
  return PrimitiveType::Int32;
}

std::string Parser::resolveImportPath(
  const std::string & import_path, const std::string & current_file)
{
  std::filesystem::path current(current_file);
  std::filesystem::path import(import_path);

  std::filesystem::path resolved = current.parent_path() / import;
  return resolved.lexically_normal().string();
}

void Parser::reportError(const std::string & message, size_t line, size_t column)
{
  std::ostringstream oss;
  oss << "Error";
  if (line > 0) {
    oss << " at line " << line;
    if (column > 0) {
      oss << ", column " << column;
    }
  }
  oss << ": " << message;
  errors_.push_back(oss.str());
}

}  // namespace fibril

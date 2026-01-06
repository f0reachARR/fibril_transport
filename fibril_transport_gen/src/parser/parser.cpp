#include "parser.hpp"

#include <fibril_transport_common/attribute_system.hpp>
#include <fibril_transport_common/ros_attribute.hpp>
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

AstStructDescriptor Parser::parseStructDefinition(TSNode node, const std::string & source)
{
  size_t line = ts_node_start_point(node).row + 1;
  size_t column = ts_node_start_point(node).column + 1;

  std::string name;
  std::vector<FieldDescriptor> fields;
  AttributeList attributes;

  uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    std::string type = ts_node_type(child);

    if (type == "attribute") {
      parseAttributeInto(child, source, attributes);
    } else if (type == "identifier") {
      name = getNodeText(child, source);
    } else if (type == "field_declaration") {
      auto field_node = parseFieldDeclaration(child, source);
      fields.push_back(std::move(field_node));
    }
  }

  StructDescriptor desc(std::move(name), std::move(fields), std::move(attributes));
  return AstStructDescriptor(std::move(desc), line, column);
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
      TSNode port_child = ts_node_child(child, 0);
      def.ports.push_back(parsePort(port_child, source));
    }
  }

  return def;
}

AstFieldDescriptor Parser::parseFieldDeclaration(TSNode node, const std::string & source)
{
  size_t line = ts_node_start_point(node).row + 1;
  size_t column = ts_node_start_point(node).column + 1;

  std::string name;
  TypeDescriptor type = TypeDescriptor::makePrimitive(PrimitiveType::Int32);
  AttributeList attributes;
  bool has_type = false;

  uint32_t count = ts_node_child_count(node);

  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    std::string child_type = ts_node_type(child);

    if (child_type == "attribute") {
      parseAttributeInto(child, source, attributes);
    } else if (child_type == "type_spec" && !has_type) {
      type = parseType(child, source);
      has_type = true;
    } else if (child_type == "identifier" && has_type) {
      name = getNodeText(child, source);
    } else if (child_type == "array_spec") {
      TSNode size_node = ts_node_child(child, 1);
      std::string size_str = getNodeText(size_node, source);
      size_t size = std::stoul(size_str);
      type = TypeDescriptor::makeArray(std::move(type), size);
    }
  }

  FieldDescriptor desc(std::move(name), std::move(type), std::move(attributes));
  return AstFieldDescriptor(std::move(desc), line, column);
}

AstPort Parser::parsePort(TSNode node, const std::string & source)
{
  std::string port_type = ts_node_type(node);
  size_t line = ts_node_start_point(node).row + 1;
  size_t column = ts_node_start_point(node).column + 1;

  AttributeList attributes;
  std::string name;
  TypeDescriptor data_type = TypeDescriptor::makePrimitive(PrimitiveType::Int32);
  TypeDescriptor request_type = TypeDescriptor::makePrimitive(PrimitiveType::Int32);
  TypeDescriptor response_type = TypeDescriptor::makePrimitive(PrimitiveType::Int32);

  uint32_t count = ts_node_child_count(node);
  int type_count = 0;

  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    std::string type = ts_node_type(child);

    if (type == "attribute") {
      parseAttributeInto(child, source, attributes);
    } else if (type == "identifier") {
      name = getNodeText(child, source);
    } else if (type == "type_spec") {
      if (port_type == "service_port") {
        if (type_count == 0) {
          request_type = parseType(child, source);
        } else {
          response_type = parseType(child, source);
        }
        type_count++;
      } else {
        data_type = parseType(child, source);
      }
    }
  }

  // Port variantを構築
  Port port_variant = ([&]() -> Port {
    if (port_type == "pub_port") {
      return PubPort(std::move(name), std::move(data_type), std::move(attributes));
    } else if (port_type == "sub_port") {
      return SubPort(std::move(name), std::move(data_type), std::move(attributes));
    } else {  // service_port
      return ServicePort(
        std::move(name), std::move(request_type), std::move(response_type), std::move(attributes));
    }
  })();

  return AstPort(std::move(port_variant), line, column);
}

TypeDescriptor Parser::parseType(TSNode node, const std::string & source)
{
  TSNode child = ts_node_child(node, 0);
  std::string child_type = ts_node_type(child);

  if (child_type == "primitive_type") {
    std::string type_name = getNodeText(child, source);
    return TypeDescriptor::makePrimitive(parsePrimitiveType(type_name));
  } else if (child_type == "qualified_identifier") {
    std::string name = getNodeText(child, source);
    return TypeDescriptor::makeStruct(name);
  }

  return TypeDescriptor::makePrimitive(PrimitiveType::Int32);
}

void Parser::parseAttributeInto(TSNode node, const std::string & source, AttributeList & attr_list)
{
  size_t line = ts_node_start_point(node).row + 1;
  size_t column = ts_node_start_point(node).column + 1;

  std::string attr_name;
  std::vector<std::string> string_args;
  std::vector<double> number_args;

  uint32_t count = ts_node_child_count(node);
  bool found_name = false;

  for (uint32_t i = 0; i < count; ++i) {
    TSNode child = ts_node_child(node, i);
    std::string type = ts_node_type(child);

    if (type == "identifier" && !found_name) {
      attr_name = getNodeText(child, source);
      found_name = true;
    } else if (type == "attribute_item") {
      TSNode item_id = ts_node_child(child, 0);
      if (ts_node_child_count(item_id) > 0 && std::string(ts_node_type(item_id)) == "identifier") {
        if (attr_name.empty()) {
          attr_name = getNodeText(item_id, source);
          found_name = true;
        }
      }
      uint32_t item_count = ts_node_child_count(child);
      for (uint32_t j = 0; j < item_count; ++j) {
        TSNode arg = ts_node_child(child, j);
        std::string arg_type = ts_node_type(arg);
        if (arg_type == "string_literal") {
          string_args.push_back(unquoteString(getNodeText(arg, source)));
        } else if (arg_type == "number_literal") {
          std::string num_str = getNodeText(arg, source);
          number_args.push_back(std::stod(num_str));
        } else if (arg_type == "qualified_identifier") {
          string_args.push_back(getNodeText(arg, source));
        }
      }
    } else if (type == "string_literal") {
      string_args.push_back(unquoteString(getNodeText(child, source)));
    } else if (type == "number_literal") {
      std::string num_str = getNodeText(child, source);
      number_args.push_back(std::stod(num_str));
    } else if (type == "qualified_identifier") {
      string_args.push_back(getNodeText(child, source));
    }
  }

  // Create appropriate attribute based on name
  using namespace fibril_transport_common;

  if (attr_name == "ros_type" && !string_args.empty()) {
    attr_list.add(std::make_unique<RosTypeAttribute>(string_args[0]));
  } else if (attr_name == "ros_map" && !string_args.empty()) {
    attr_list.add(std::make_unique<RosMapAttribute>(string_args[0]));
  } else if (attr_name == "ros" && !string_args.empty()) {
    attr_list.add(std::make_unique<RosAttribute>(string_args[0]));
  } else if (attr_name == "unit" && !string_args.empty()) {
    attr_list.add(std::make_unique<UnitAttribute>(string_args[0]));
  } else if (attr_name == "ros_frame_id" && !string_args.empty()) {
    attr_list.add(std::make_unique<RosFrameIdAttribute>(string_args[0]));
  } else if (attr_name == "ros_service" && !string_args.empty()) {
    attr_list.add(std::make_unique<RosServiceAttribute>(string_args[0]));
  } else {
    reportError("Unknown attribute: " + attr_name, line, column);
  }
}

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

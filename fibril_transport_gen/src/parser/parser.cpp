#include "parser.hpp"

#include <fibril_transport_common/attribute_system.hpp>
#include <fibril_transport_common/ros_attribute.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

#include "attribute_parser.hpp"
#include "ros_attribute_parser.hpp"

namespace fibril
{

Parser::Parser()
{
  parser_ = ts_parser_new();
  language_ = tree_sitter_fibril();
  ts_parser_set_language(parser_, language_);

  registerBuiltinAttributeParsers();
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

bool Parser::hasTreeSitterErrors(TSNode node)
{
  if (ts_node_is_missing(node)) {
    return true;
  }

  uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    if (hasTreeSitterErrors(ts_node_child(node, i))) {
      return true;
    }
  }
  return false;
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

  // Check for Tree-sitter syntax errors first
  if (hasTreeSitterErrors(root)) {
    reportError("Syntax errors detected in source file", 0, 0);
    return sf;
  }

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
  NodeCursor cursor(node, source, [this](const std::string & msg, size_t line, size_t col) {
    this->reportError(msg, line, col);
  });

  SyntaxDeclaration decl;
  auto [line, column] = cursor.currentLocation();
  decl.line = line;
  decl.column = column;

  cursor.expect("syntax");
  cursor.expect("=");

  // Expect string literal (mandatory)
  if (auto str_node = cursor.expect("string_literal")) {
    decl.version = unquoteString(getNodeText(*str_node, source));
  }

  cursor.expect(";");
  cursor.expectEnd();
  return decl;
}

PackageDeclaration Parser::parsePackageDeclaration(TSNode node, const std::string & source)
{
  NodeCursor cursor(node, source, [this](const std::string & msg, size_t line, size_t col) {
    this->reportError(msg, line, col);
  });

  PackageDeclaration decl;
  auto [line, column] = cursor.currentLocation();
  decl.line = line;
  decl.column = column;

  // Expect "package" keyword
  cursor.expect("package");

  // Expect qualified identifier (mandatory)
  if (auto id_node = cursor.expect("qualified_identifier")) {
    decl.name = getNodeText(*id_node, source);
  }

  cursor.expect(";");
  cursor.expectEnd();
  return decl;
}

ImportDeclaration Parser::parseImportDeclaration(TSNode node, const std::string & source)
{
  NodeCursor cursor(node, source, [this](const std::string & msg, size_t line, size_t col) {
    this->reportError(msg, line, col);
  });

  ImportDeclaration decl;
  auto [line, column] = cursor.currentLocation();
  decl.line = line;
  decl.column = column;

  // Expect "import" keyword
  cursor.expect("import");

  // Expect string literal (mandatory)
  if (auto str_node = cursor.expect("string_literal")) {
    decl.path = unquoteString(getNodeText(*str_node, source));
  }

  cursor.expectEnd();
  return decl;
}

AstStructDescriptor Parser::parseStructDefinition(TSNode node, const std::string & source)
{
  NodeCursor cursor(node, source, [this](const std::string & msg, size_t line, size_t col) {
    this->reportError(msg, line, col);
  });

  auto [line, column] = cursor.currentLocation();

  AttributeList attributes;
  std::optional<std::string> name;
  std::vector<FieldDescriptor> fields;

  // Consume all attributes (0 or more)
  for (auto attr_node : cursor.eatAll("attribute")) {
    parseAttributeInto(attr_node, source, attributes);
  }

  // Expect "struct" keyword
  cursor.expect("struct");

  // Expect struct name (mandatory)
  if (auto id_node = cursor.expect("identifier")) {
    name = getNodeText(*id_node, source);
  } else {
    // Error already reported by expect()
    name = "UnnamedStruct";  // Fallback for recovery
  }

  // Expect opening brace
  cursor.expect("{");

  // Consume all field declarations (0 or more)
  for (auto field_node : cursor.eatAll("field_declaration")) {
    auto field = parseFieldDeclaration(field_node, source);
    fields.push_back(std::move(field.get()));
  }

  // Expect closing brace
  cursor.expect("}");

  // Check for unconsumed nodes
  cursor.expectEnd();

  StructDescriptor desc(std::move(*name), std::move(fields), std::move(attributes));
  return AstStructDescriptor(std::move(desc), line, column);
}

NodeDefinition Parser::parseNodeDefinition(TSNode node, const std::string & source)
{
  NodeCursor cursor(node, source, [this](const std::string & msg, size_t line, size_t col) {
    this->reportError(msg, line, col);
  });

  NodeDefinition def;
  auto [line, column] = cursor.currentLocation();
  def.line = line;
  def.column = column;

  // Expect "node" keyword (if grammar includes it)
  cursor.eat("node");

  // Expect node name (mandatory)
  if (auto id_node = cursor.expect("identifier")) {
    def.name = getNodeText(*id_node, source);
  } else {
    // Error already reported by expect()
    def.name = "UnnamedNode";  // Fallback for recovery
  }

  // Expect opening brace
  cursor.expect("{");

  // Consume all port declarations (0 or more)
  for (auto port_decl_node : cursor.eatAll("port_declaration")) {
    TSNode port_child = ts_node_child(port_decl_node, 0);
    def.ports.push_back(parsePort(port_child, source));
  }

  // Expect closing brace
  cursor.expect("}");

  // Check for unconsumed nodes
  cursor.expectEnd();

  return def;
}

AstFieldDescriptor Parser::parseFieldDeclaration(TSNode node, const std::string & source)
{
  NodeCursor cursor(node, source, [this](const std::string & msg, size_t line, size_t col) {
    this->reportError(msg, line, col);
  });

  auto [line, column] = cursor.currentLocation();

  AttributeList attributes;
  std::optional<TypeDescriptor> type;
  std::optional<std::string> name;

  // Consume all attributes (0 or more)
  for (auto attr_node : cursor.eatAll("attribute")) {
    parseAttributeInto(attr_node, source, attributes);
  }

  // Expect type specification (mandatory)
  if (auto type_node = cursor.expect("type_spec")) {
    type = parseType(*type_node, source);
  } else {
    // Error already reported by expect()
    type = TypeDescriptor::makePrimitive(PrimitiveType::Int32);  // Fallback for recovery
  }

  // Expect identifier (mandatory)
  if (auto id_node = cursor.expect("identifier")) {
    name = getNodeText(*id_node, source);
  } else {
    // Error already reported by expect()
    name = "unnamed";  // Fallback for recovery
  }

  // Optional: array specification
  if (auto array_node = cursor.eat("array_spec")) {
    TSNode size_node = ts_node_child(*array_node, 1);
    std::string size_str = getNodeText(size_node, source);
    size_t size = std::stoul(size_str);
    type = TypeDescriptor::makeArray(std::move(*type), size);
  }

  cursor.expect(";");

  // Check for unconsumed nodes
  cursor.expectEnd();

  FieldDescriptor desc(std::move(*name), std::move(*type), std::move(attributes));
  return AstFieldDescriptor(std::move(desc), line, column);
}

AstPort Parser::parsePort(TSNode node, const std::string & source)
{
  NodeCursor cursor(node, source, [this](const std::string & msg, size_t line, size_t col) {
    this->reportError(msg, line, col);
  });

  std::string port_type = ts_node_type(node);
  auto [line, column] = cursor.currentLocation();

  AttributeList attributes;
  std::optional<std::string> name;
  std::optional<TypeDescriptor> data_type;
  std::optional<TypeDescriptor> request_type;
  std::optional<TypeDescriptor> response_type;

  // Consume all attributes (0 or more)
  for (auto attr_node : cursor.eatAll("attribute")) {
    parseAttributeInto(attr_node, source, attributes);
  }

  // Grammar structure differs between port types:
  // sub_port:     repeat(attribute) 'sub' type_spec identifier ';'
  // pub_port:     repeat(attribute) 'pub' type_spec identifier ';'
  // service_port: repeat(attribute) 'service' identifier '(' optional(type_spec) ')' '->' (type_spec|'void') ';'

  if (port_type == "service_port") {
    // Service port: service IDENTIFIER ( type_spec? ) -> (type_spec|void) ;

    // Expect "service" keyword
    cursor.expect("service");

    // Expect port name first (mandatory for service)
    if (auto id_node = cursor.expect("identifier")) {
      name = getNodeText(*id_node, source);
    } else {
      name = "unnamed_port";  // Fallback
    }

    // Skip '('
    cursor.expect("(");

    // Request type is optional in grammar
    if (auto req_type_node = cursor.eat("type_spec")) {
      request_type = parseType(*req_type_node, source);
    }
    // If no request type, it means empty request (like void)

    // Skip ')'
    cursor.expect(")");

    // Skip '->'
    if (cursor.eat("->")) {
      // Response type can be type_spec or 'void' keyword
      if (auto resp_type_node = cursor.eat("type_spec")) {
        response_type = parseType(*resp_type_node, source);
      } else {
        // Could be 'void' keyword - for now we don't have a dedicated Void type
        // We'll use an empty/default type or add a Void primitive type
        // For now, create an Int32 as fallback
        cursor.expect("void");  // Try to consume void keyword if present
        response_type = TypeDescriptor::makePrimitive(PrimitiveType::Int32);
      }
    }

    // If request_type wasn't set, also use default
    if (!request_type) {
      request_type = TypeDescriptor::makePrimitive(PrimitiveType::Int32);
    }

  } else {
    // Pub/Sub port: (pub|sub) TYPE_SPEC IDENTIFIER ;

    // Expect port keyword (pub/sub)
    cursor.expect(port_type == "pub_port" ? "pub" : "sub");

    // Expect type first
    if (auto type_node = cursor.expect("type_spec")) {
      data_type = parseType(*type_node, source);
    } else {
      data_type = TypeDescriptor::makePrimitive(PrimitiveType::Int32);  // Fallback
    }

    // Then expect name
    if (auto id_node = cursor.expect("identifier")) {
      name = getNodeText(*id_node, source);
    } else {
      name = "unnamed_port";  // Fallback
    }
  }

  // Skip semicolon
  cursor.expect(";");

  // Check for unconsumed nodes
  cursor.expectEnd();

  // Construct Port variant
  Port port_variant = ([&]() -> Port {
    if (port_type == "pub_port") {
      return PubPort(std::move(*name), std::move(*data_type), std::move(attributes));
    } else if (port_type == "sub_port") {
      return SubPort(std::move(*name), std::move(*data_type), std::move(attributes));
    } else {  // service_port
      return ServicePort(
        std::move(*name), std::move(*request_type), std::move(*response_type),
        std::move(attributes));
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
  NodeCursor cursor(node, source, [this](const std::string & msg, size_t line, size_t col) {
    this->reportError(msg, line, col);
  });

  auto [line, column] = cursor.currentLocation();

  cursor.expect("#[");

  std::string attr_name;

  if (auto identifier = cursor.expect("identifier")) {
    attr_name = getNodeText(*identifier, source);
  }

  AttributeParameter params;

  if (cursor.eat("(")) {
    while (auto param_node =
             cursor.eatAny({"string_literal", "number_literal", "qualified_identifier"})) {
      std::string type = ts_node_type(*param_node);
      std::string value = getNodeText(*param_node, source);
      if (type == "string_literal") {
        params.values.push_back(AttributeParameter::String(value));
      } else if (type == "number_literal") {
        if (value.find('.') != std::string::npos) {
          params.values.push_back(AttributeParameter::FloatingPoint(std::stof(value)));
        } else {
          params.values.push_back(AttributeParameter::Integer(std::stoi(value)));
        }
      } else if (type == "qualified_identifier") {
        params.values.push_back(AttributeParameter::Identifier(value));
      }
    }
    cursor.expect(")");
  }

  try {
    auto parsed_attribute = AttributeParserRegistry::parse(attr_name, params);
    if (parsed_attribute) {
      attr_list.add(std::move(parsed_attribute));
    } else {
      reportError("Unknown attribute: " + attr_name, line, column);
    }
  } catch (const std::exception & e) {
    reportError("Parsing while parameter of " + attr_name + " causes " + e.what(), line, column);
  }

  cursor.expect("]");
  cursor.expectEnd();
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

  // Unknown primitive type - report error instead of defaulting silently
  reportError("Unknown primitive type: " + type_name, 0, 0);
  return PrimitiveType::Int32;  // Fallback for recovery
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

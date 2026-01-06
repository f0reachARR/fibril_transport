#include "node_cursor.hpp"

#include <algorithm>

namespace fibril
{

NodeCursor::NodeCursor(TSNode parent, const std::string & source, ErrorReporter error_reporter)
: parent_(parent),
  source_(source),
  error_reporter_(std::move(error_reporter)),
  current_index_(0),
  child_count_(ts_node_child_count(parent))
{
}

std::optional<TSNode> NodeCursor::peek() const
{
  if (isAtEnd()) {
    return std::nullopt;
  }
  return ts_node_child(parent_, current_index_);
}

std::optional<TSNode> NodeCursor::advance()
{
  if (isAtEnd()) {
    return std::nullopt;
  }
  TSNode node = ts_node_child(parent_, current_index_);
  current_index_++;
  return node;
}

std::optional<TSNode> NodeCursor::eat(const std::string & node_type)
{
  skipUnnamed();

  auto current = peek();
  if (!current) {
    return std::nullopt;
  }

  std::string type = ts_node_type(*current);
  if (type == node_type) {
    return advance();
  }

  return std::nullopt;
}

std::optional<TSNode> NodeCursor::expect(const std::string & node_type)
{
  skipUnnamed();

  auto current = peek();
  if (!current) {
    auto [line, col] = currentLocation();
    error_reporter_("Expected '" + node_type + "' but reached end of node", line, col);
    return std::nullopt;
  }

  std::string type = ts_node_type(*current);
  if (type != node_type) {
    auto [line, col] = locationOf(*current);
    error_reporter_("Expected '" + node_type + "' but found '" + type + "'", line, col);
    // Don't consume the unexpected node, let caller decide what to do
    return std::nullopt;
  }

  return advance();
}

std::optional<TSNode> NodeCursor::eatAny(const std::vector<std::string> & node_types)
{
  skipUnnamed();

  auto current = peek();
  if (!current) {
    return std::nullopt;
  }

  std::string type = ts_node_type(*current);
  if (std::find(node_types.begin(), node_types.end(), type) != node_types.end()) {
    return advance();
  }

  return std::nullopt;
}

std::vector<TSNode> NodeCursor::eatAll(const std::string & node_type)
{
  std::vector<TSNode> nodes;

  while (auto node = eat(node_type)) {
    nodes.push_back(*node);
  }

  return nodes;
}

std::optional<std::string> NodeCursor::currentType() const
{
  auto current = peek();
  if (!current) {
    return std::nullopt;
  }

  return std::string(ts_node_type(*current));
}

std::pair<size_t, size_t> NodeCursor::currentLocation() const
{
  auto current = peek();
  if (!current) {
    // Return parent's end location if we're past all children
    TSPoint end = ts_node_end_point(parent_);
    return {end.row + 1, end.column + 1};
  }

  return locationOf(*current);
}

std::pair<size_t, size_t> NodeCursor::locationOf(TSNode node)
{
  TSPoint start = ts_node_start_point(node);
  return {start.row + 1, start.column + 1};
}

void NodeCursor::expectEnd()
{
  skipUnnamed();

  while (!isAtEnd()) {
    auto node = advance();
    if (node) {
      std::string type = ts_node_type(*node);

      // Skip certain ignorable node types that might be injected by tree-sitter
      // Common examples: comments, whitespace nodes
      // For now, report all unconsumed nodes for strictness
      auto [line, col] = locationOf(*node);
      error_reporter_(
        "Unexpected node '" + type + "' (all expected nodes already consumed)", line, col);
    }
  }
}

void NodeCursor::skipUnnamed()
{
  while (!isAtEnd()) {
    auto current = peek();
    if (current && !ts_node_is_named(*current)) {
      advance();
    } else {
      break;
    }
  }
}

std::string NodeCursor::getNodeText(TSNode node) const
{
  uint32_t start = ts_node_start_byte(node);
  uint32_t end = ts_node_end_byte(node);
  return source_.substr(start, end - start);
}

}  // namespace fibril

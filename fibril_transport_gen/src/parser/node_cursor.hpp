#pragma once

#include <tree_sitter/api.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fibril
{

/**
 * @brief Helper class for traversing Tree-sitter AST nodes with explicit consumption
 * 
 * NodeCursor provides lexer-like eat/expect methods for parsing Tree-sitter AST,
 * making the parsing logic more explicit and catching unexpected nodes early.
 */
class NodeCursor
{
public:
  /**
   * @brief Error reporting callback type
   * 
   * Parameters: (message, line, column)
   */
  using ErrorReporter = std::function<void(const std::string &, size_t, size_t)>;

  /**
   * @brief Construct a cursor for traversing child nodes of a parent
   * 
   * @param parent Parent TSNode whose children will be traversed
   * @param source Source code string for extracting node text
   * @param error_reporter Callback for reporting errors
   */
  NodeCursor(TSNode parent, const std::string & source, ErrorReporter error_reporter);

  /**
   * @brief Peek at current node without advancing cursor
   * 
   * @return Current node if not at end, nullopt otherwise
   */
  std::optional<TSNode> peek() const;

  /**
   * @brief Consume current node and advance cursor
   * 
   * @return Consumed node if not at end, nullopt otherwise
   */
  std::optional<TSNode> advance();

  /**
   * @brief Consume node if it matches expected type
   * 
   * @param node_type Expected node type
   * @return Consumed node if type matches, nullopt otherwise
   */
  std::optional<TSNode> eat(const std::string & node_type);

  /**
   * @brief Consume node, report error if type doesn't match
   * 
   * @param node_type Expected node type
   * @return Consumed node if type matches, nullopt if at end or type mismatch (error reported)
   */
  std::optional<TSNode> expect(const std::string & node_type);

  /**
   * @brief Consume node if it matches any of the expected types
   * 
   * @param node_types List of acceptable node types
   * @return Consumed node if type matches any, nullopt otherwise
   */
  std::optional<TSNode> eatAny(const std::vector<std::string> & node_types);

  /**
   * @brief Consume all consecutive nodes of given type
   * 
   * @param node_type Expected node type
   * @return Vector of all consumed nodes (may be empty)
   */
  std::vector<TSNode> eatAll(const std::string & node_type);

  /**
   * @brief Get type of current node
   * 
   * @return Node type string if not at end, nullopt otherwise
   */
  std::optional<std::string> currentType() const;

  /**
   * @brief Get line and column of current node for error reporting
   * 
   * @return Pair of (line, column), both 1-indexed
   */
  std::pair<size_t, size_t> currentLocation() const;

  /**
   * @brief Get line and column of specific node
   * 
   * @param node Node to get location from
   * @return Pair of (line, column), both 1-indexed
   */
  static std::pair<size_t, size_t> locationOf(TSNode node);

  /**
   * @brief Check if all nodes have been consumed
   * 
   * @return true if cursor is at end, false otherwise
   */
  bool isAtEnd() const { return current_index_ >= child_count_; }

  /**
   * @brief Verify all nodes consumed, skip and report unconsumed nodes
   * 
   * Reports errors for any remaining nodes that haven't been consumed.
   * This helps catch grammar changes or unexpected node structures.
   */
  void expectEnd();

  /**
   * @brief Skip unnamed nodes (whitespace, comments, etc.)
   * 
   * Tree-sitter may include unnamed nodes for formatting. This method
   * advances past all consecutive unnamed nodes.
   */
  void skipUnnamed();

private:
  TSNode parent_;
  const std::string & source_;
  ErrorReporter error_reporter_;
  uint32_t current_index_;
  uint32_t child_count_;

  /**
   * @brief Get text content of a node
   */
  std::string getNodeText(TSNode node) const;
};

}  // namespace fibril

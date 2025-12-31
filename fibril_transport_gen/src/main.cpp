#include <iostream>

#include "parser/parser.hpp"

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input.fibril>" << std::endl;
    return 1;
  }

  std::string input_file = argv[1];

  fibril::Parser parser;
  auto ast = parser.parse(input_file);

  if (!parser.getErrors().empty()) {
    std::cerr << "Parsing errors:" << std::endl;
    for (const auto & err : parser.getErrors()) {
      std::cerr << "  " << err << std::endl;
    }
    return 1;
  }

  std::cout << "Successfully parsed: " << input_file << std::endl;
  std::cout << "  Package: " << ast.package.name << std::endl;
  std::cout << "  Structs: " << ast.structs.size() << std::endl;
  std::cout << "  Nodes: " << ast.nodes.size() << std::endl;

  // TODO: 実際のコード生成処理

  return 0;
}

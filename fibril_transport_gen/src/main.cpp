#include <filesystem>
#include <fstream>
#include <iostream>

#include "generator/cpp_generator.hpp"
#include "parser/parser.hpp"
#include "validator/validator.hpp"

void print_usage(const char * program_name)
{
  std::cerr << "Usage: " << program_name << " <input.fibril> [-o <output_dir>]\n";
  std::cerr << "\nOptions:\n";
  std::cerr << "  -o <output_dir>  Output directory (default: current directory)\n";
  std::cerr << "  -h, --help       Show this help message\n";
}

int main(int argc, char ** argv)
{
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  std::string input_file;
  std::string output_dir = ".";

  // 引数解析
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return 0;
    } else if (arg == "-o") {
      if (i + 1 < argc) {
        output_dir = argv[++i];
      } else {
        std::cerr << "Error: -o requires an argument\n";
        return 1;
      }
    } else if (input_file.empty()) {
      input_file = arg;
    } else {
      std::cerr << "Error: Unexpected argument: " << arg << "\n";
      return 1;
    }
  }

  if (input_file.empty()) {
    std::cerr << "Error: No input file specified\n";
    print_usage(argv[0]);
    return 1;
  }

  // パース
  fibril::Parser parser;
  auto ast = parser.parse(input_file);

  if (!parser.getErrors().empty()) {
    std::cerr << "Parse errors:\n";
    for (const auto & error : parser.getErrors()) {
      std::cerr << "  " << error << "\n";
    }
    return 1;
  }

  // バリデーション
  fibril::Validator validator;
  if (!validator.validate(ast)) {
    std::cerr << "Validation errors:\n";
    for (const auto & error : validator.getErrors()) {
      std::cerr << "  " << error.format() << "\n";
    }
    return 1;
  }

  // ノードがあるか確認
  if (ast.nodes.empty()) {
    std::cerr << "Error: No nodes found in " << input_file << "\n";
    return 1;
  }

  if (ast.nodes.size() > 1) {
    std::cerr << "Warning: Multiple nodes found, generating code for first node only\n";
  }

  // コード生成
  fibril::CppGenerator generator;
  auto result = generator.generate(ast.nodes[0], ast);

  // 出力ファイル名
  std::filesystem::path output_path(output_dir);
  std::filesystem::create_directories(output_path);

  std::string output_file = (output_path / (result.node_name + "_node.hpp")).string();

  // ファイル書き込み
  std::ofstream out(output_file);
  if (!out.is_open()) {
    std::cerr << "Error: Cannot write to " << output_file << "\n";
    return 1;
  }

  out << result.header_content;
  out.close();

  std::cout << "Generated: " << output_file << "\n";
  std::cout << "  Node: " << result.node_name << "\n";
  std::cout << "  Data size: " << result.data_size << " bytes\n";
  std::cout << "  MAX_RX_MAPPINGS: " << result.max_rx_mappings << "\n";
  std::cout << "  MAX_TX_MAPPINGS: " << result.max_tx_mappings << "\n";

  return 0;
}

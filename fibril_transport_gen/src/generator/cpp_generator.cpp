#include "cpp_generator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace fibril
{

CppGenerator::CppGenerator(const CodeGenOptions & options) : options_(options) {}

GeneratedCode CppGenerator::generate(const NodeDefinition & node, const SourceFile & source)
{
  GeneratedCode result;
  result.node_name = node.name;

  std::ostringstream ss;

  // ヘッダーガード開始
  ss << generateHeaderGuard(node.name, true) << "\n\n";

  // インクルード
  ss << generateIncludes() << "\n\n";

  // リソース計算
  result.data_size = calculateDataSize(node, source);
  result.max_rx_mappings = calculateMaxMappings(node.ports, true, source);
  result.max_tx_mappings = calculateMaxMappings(node.ports, false, source);

  // IDMapping構造体
  ss << generateIDMappingStruct() << "\n\n";

  // Service列挙型
  if (!node.ports.empty()) {
    std::string service_enum = generateServiceEnum(node);
    if (!service_enum.empty()) {
      ss << service_enum << "\n\n";
    }
  }

  // Service構造体
  std::string service_structs = generateServiceStructs(node, source);
  if (!service_structs.empty()) {
    ss << service_structs << "\n\n";
  }

  // ノードクラス
  ss << generateNodeClass(node, source) << "\n\n";

  // ヘッダーガード終了
  ss << generateHeaderGuard(node.name, false) << "\n";

  result.header_content = ss.str();
  return result;
}

std::string CppGenerator::generateHeaderGuard(const std::string & node_name, bool opening)
{
  std::string guard = toUpperSnakeCase(node_name) + "_NODE_HPP";

  if (opening) {
    return "#ifndef " + guard + "\n#define " + guard;
  } else {
    return "#endif // " + guard;
  }
}

std::string CppGenerator::generateIncludes()
{
  return R"(#include <stdint.h>
#include <string.h>)";
}

std::string CppGenerator::generateDataStruct(const NodeDefinition & node, const SourceFile & source)
{
  std::ostringstream ss;

  ss << "    // Data structure (packed for direct CAN frame mapping)\n";
  ss << "    struct __attribute__((packed)) {\n";

  // Sub ports (RX) を先に
  for (const auto & port : node.ports) {
    if (isSubPort(port)) {
      const auto & sub = std::get<SubPort>(port);
      std::string cpp_type = toCppType(sub.data_type);
      ss << "        " << cpp_type << " " << sub.name << ";\n";
    }
  }

  // Pub ports (TX) を後に
  for (const auto & port : node.ports) {
    if (isPubPort(port)) {
      const auto & pub = std::get<PubPort>(port);
      std::string cpp_type = toCppType(pub.data_type);
      ss << "        " << cpp_type << " " << pub.name << ";\n";
    }
  }

  ss << "    } data;";

  return ss.str();
}

std::string CppGenerator::generateIDMappingStruct()
{
  return R"(// ID Mapping structure for CAN frame routing
struct IDMapping {
    uint16_t can_id;        // CAN ID
    uint16_t address;       // Data structure offset
    uint8_t offset;         // Payload offset within CAN frame
    uint8_t length;         // Data length in bytes
    bool is_completion;     // Completion flag for multi-frame
};)";
}

std::string CppGenerator::generateServiceEnum(const NodeDefinition & node)
{
  std::vector<std::string> services;

  for (const auto & port : node.ports) {
    if (isServicePort(port)) {
      services.push_back(getPortName(port));
    }
  }

  if (services.empty()) {
    return "";
  }

  std::ostringstream ss;
  ss << "// Service IDs\n";
  ss << "enum ServiceID : uint8_t {\n";

  for (size_t i = 0; i < services.size(); ++i) {
    std::string enum_name = "SERVICE_" + toUpperSnakeCase(services[i]);
    ss << "    " << enum_name << " = " << i;
    if (i < services.size() - 1) {
      ss << ",";
    }
    ss << "\n";
  }

  ss << "};";

  return ss.str();
}

std::string CppGenerator::generateServiceStructs(
  const NodeDefinition & node, const SourceFile & source)
{
  std::ostringstream ss;
  bool has_service = false;

  for (const auto & port : node.ports) {
    if (isServicePort(port)) {
      const auto & svc = std::get<ServicePort>(port);
      has_service = true;

      // Request構造体
      std::string req_type = toCppType(svc.request_type);
      std::string res_type = toCppType(svc.response_type);

      std::string struct_name = toPascalCase(svc.name);

      if (!svc.request_type.isPrimitive() || req_type != "void") {
        ss << "// " << struct_name << " Request\n";
        ss << "struct " << struct_name << "Request {\n";
        ss << "    " << req_type << " data;\n";
        ss << "};\n\n";
      }

      if (!svc.response_type.isPrimitive() || res_type != "void") {
        ss << "// " << struct_name << " Response\n";
        ss << "struct " << struct_name << "Response {\n";
        ss << "    " << res_type << " data;\n";
        ss << "};\n\n";
      }
    }
  }

  if (!has_service) {
    return "";
  }

  return ss.str();
}

std::string CppGenerator::generateNodeClass(const NodeDefinition & node, const SourceFile & source)
{
  std::ostringstream ss;
  std::string class_name = toPascalCase(node.name) + "Node";

  size_t max_rx = calculateMaxMappings(node.ports, true, source);
  size_t max_tx = calculateMaxMappings(node.ports, false, source);
  size_t data_size = calculateDataSize(node, source);

  ss << "class " << class_name << " {\n";
  ss << "public:\n";

  // コンストラクタ
  ss << "    " << class_name << "() {\n";
  ss << "        memset(&data, 0, sizeof(data));\n";
  ss << "    }\n\n";

  // リソース定数
  ss << "    // Resource sizing (auto-calculated)\n";
  ss << "    static constexpr uint8_t MAX_RX_MAPPINGS = " << max_rx << ";\n";
  ss << "    static constexpr uint8_t MAX_TX_MAPPINGS = " << max_tx << ";\n\n";

  if (options_.generate_memory_estimates) {
    ss << "    // Memory estimates:\n";
    ss << "    //   Data structure: " << data_size << " bytes\n";
    ss << "    //   ID mappings: " << ((max_rx + max_tx) * 8) << " bytes\n";
    ss << "    //   Total: " << (data_size + (max_rx + max_tx) * 8) << " bytes\n\n";
  }

  // データ構造
  ss << generateDataStruct(node, source) << "\n\n";

  // IDマッピング配列
  ss << "    // ID Mapping arrays\n";
  ss << "    IDMapping rx_map[MAX_RX_MAPPINGS];\n";
  ss << "    IDMapping tx_map[MAX_TX_MAPPINGS];\n";
  ss << "    uint8_t rx_mapping_count = 0;\n";
  ss << "    uint8_t tx_mapping_count = 0;\n\n";

  // Configuration メソッド
  ss << "    // Configuration methods\n";
  ss << "    bool configure_rx_mapping(uint16_t can_id, uint16_t address, uint8_t offset,\n";
  ss << "                             uint8_t length, bool is_completion) {\n";
  ss << "        if (rx_mapping_count >= MAX_RX_MAPPINGS) return false;\n";
  ss << "        rx_map[rx_mapping_count].can_id = can_id;\n";
  ss << "        rx_map[rx_mapping_count].address = address;\n";
  ss << "        rx_map[rx_mapping_count].offset = offset;\n";
  ss << "        rx_map[rx_mapping_count].length = length;\n";
  ss << "        rx_map[rx_mapping_count].is_completion = is_completion;\n";
  ss << "        rx_mapping_count++;\n";
  ss << "        return true;\n";
  ss << "    }\n\n";

  ss << "    bool configure_tx_mapping(uint16_t can_id, uint16_t address, uint8_t offset,\n";
  ss << "                             uint8_t length, bool is_completion) {\n";
  ss << "        if (tx_mapping_count >= MAX_TX_MAPPINGS) return false;\n";
  ss << "        tx_map[tx_mapping_count].can_id = can_id;\n";
  ss << "        tx_map[tx_mapping_count].address = address;\n";
  ss << "        tx_map[tx_mapping_count].offset = offset;\n";
  ss << "        tx_map[tx_mapping_count].length = length;\n";
  ss << "        tx_map[tx_mapping_count].is_completion = is_completion;\n";
  ss << "        tx_mapping_count++;\n";
  ss << "        return true;\n";
  ss << "    }\n\n";

  // CAN受信ハンドラ
  ss << "    // CAN receive handler\n";
  ss << "    void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) {\n";
  ss << "        for (uint8_t i = 0; i < rx_mapping_count; i++) {\n";
  ss << "            if (rx_map[i].can_id == can_id) {\n";
  ss << "                uint8_t* dest = (uint8_t*)&data + rx_map[i].address;\n";
  ss << "                memcpy(dest, payload + rx_map[i].offset, rx_map[i].length);\n";
  ss << "                \n";
  ss << "                if (rx_map[i].is_completion) {\n";
  ss << "                    on_data_received(rx_map[i].address);\n";
  ss << "                }\n";
  ss << "                return;\n";
  ss << "            }\n";
  ss << "        }\n";
  ss << "    }\n\n";

  // 仮想コールバック
  ss << "    // Virtual callbacks (override in derived class)\n";
  ss << "    virtual void on_data_received(uint16_t address) {}\n";

  ss << "};\n";

  return ss.str();
}

std::string CppGenerator::toCppType(const Type & type)
{
  return std::visit(
    [this](const auto & t) -> std::string {
      using T = std::decay_t<decltype(t)>;

      if constexpr (std::is_same_v<T, PrimitiveType>) {
        switch (t) {
          case PrimitiveType::Bool:
            return "bool";
          case PrimitiveType::Int8:
            return "int8_t";
          case PrimitiveType::UInt8:
            return "uint8_t";
          case PrimitiveType::Int16:
            return "int16_t";
          case PrimitiveType::UInt16:
            return "uint16_t";
          case PrimitiveType::Int32:
            return "int32_t";
          case PrimitiveType::UInt32:
            return "uint32_t";
          case PrimitiveType::Int64:
            return "int64_t";
          case PrimitiveType::UInt64:
            return "uint64_t";
          case PrimitiveType::Float:
            return "float";
          case PrimitiveType::Double:
            return "double";
          default:
            return "int32_t";
        }
      } else if constexpr (std::is_same_v<T, StructType>) {
        return t.name;
      } else if constexpr (std::is_same_v<T, ArrayType>) {
        std::string elem_type = toCppType(*t.element_type);
        return elem_type + "[" + std::to_string(t.size) + "]";
      }

      return "void";
    },
    type.value);
}

std::string CppGenerator::toSnakeCase(const std::string & str)
{
  std::string result;
  for (size_t i = 0; i < str.length(); i++) {
    if (i > 0 && std::isupper(str[i]) && std::islower(str[i - 1])) {
      result += '_';
    }
    result += std::tolower(str[i]);
  }
  return result;
}

std::string CppGenerator::toUpperSnakeCase(const std::string & str)
{
  std::string snake = toSnakeCase(str);
  std::transform(snake.begin(), snake.end(), snake.begin(), ::toupper);
  return snake;
}

std::string CppGenerator::toPascalCase(const std::string & str)
{
  std::string result;
  bool capitalize_next = true;

  for (char c : str) {
    if (c == '_' || c == '-') {
      capitalize_next = true;
    } else {
      result += capitalize_next ? std::toupper(c) : c;
      capitalize_next = false;
    }
  }

  return result;
}

size_t CppGenerator::calculateMaxMappings(
  const std::vector<Port> & ports, bool is_rx, const SourceFile & source)
{
  size_t total = 0;

  for (const auto & port : ports) {
    bool matches = is_rx ? isSubPort(port) : isPubPort(port);
    if (!matches) continue;

    const Type & data_type = getPortDataType(port);
    size_t data_size = size_calc_.calculateSize(data_type, source);

    // フレーム分割数 = ceil(データサイズ / CAN_FRAME_SIZE)
    size_t num_frames = (data_size + options_.can_frame_size - 1) / options_.can_frame_size;
    total += num_frames;
  }

  return total > 0 ? total : 1;  // 最低1
}

size_t CppGenerator::calculateDataSize(const NodeDefinition & node, const SourceFile & source)
{
  size_t total = 0;

  for (const auto & port : node.ports) {
    if (isPubPort(port) || isSubPort(port)) {
      const Type & data_type = getPortDataType(port);
      total += size_calc_.calculateSize(data_type, source);
    }
  }

  return total;
}

bool CppGenerator::isPubPort(const Port & port) { return std::holds_alternative<PubPort>(port); }

bool CppGenerator::isSubPort(const Port & port) { return std::holds_alternative<SubPort>(port); }

bool CppGenerator::isServicePort(const Port & port)
{
  return std::holds_alternative<ServicePort>(port);
}

std::string CppGenerator::getPortName(const Port & port)
{
  return std::visit([](const auto & p) { return p.name; }, port);
}

const Type & CppGenerator::getPortDataType(const Port & port)
{
  return std::visit(
    [](const auto & p) -> const Type & {
      using T = std::decay_t<decltype(p)>;
      if constexpr (std::is_same_v<T, PubPort> || std::is_same_v<T, SubPort>) {
        return p.data_type;
      }
      // ServicePortの場合はダミーを返す（呼ばれないはず）
      static Type dummy = Type::makePrimitive(PrimitiveType::Int32, 0, 0);
      return dummy;
    },
    port);
}

}  // namespace fibril

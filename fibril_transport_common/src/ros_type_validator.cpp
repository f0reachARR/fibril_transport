#include "fibril_transport_common/ros_type_validator.hpp"

#include <algorithm>
#include <sstream>

namespace fibril
{
// MessageTypeを文字列に変換
std::string ROSTypeValidator::messageTypeToString(ros_babel_fish::MessageType type)
{
  using namespace ros_babel_fish::MessageTypes;

  switch (type) {
    case Float:
      return "float";
    case Double:
      return "double";
    case LongDouble:
      return "long double";
    case Bool:
      return "bool";
    case Char:
      return "char";
    case WChar:
      return "wchar";
    case Octet:
      return "octet";
    case UInt8:
      return "uint8";
    case Int8:
      return "int8";
    case UInt16:
      return "uint16";
    case Int16:
      return "int16";
    case UInt32:
      return "uint32";
    case Int32:
      return "int32";
    case UInt64:
      return "uint64";
    case Int64:
      return "int64";
    case String:
      return "string";
    case WString:
      return "wstring";
    case Compound:
      return "compound";
    case Array:
      return "array";
    default:
      return "unknown";
  }
}

ROSTypeValidator::ROSTypeValidator() { initialize(); }

ROSTypeValidator::~ROSTypeValidator() = default;

void ROSTypeValidator::initialize()
{
  // ROS 2ノード初期化（既に初期化済みの場合はスキップ）
  if (!rclcpp::ok()) {
    rclcpp::init(0, nullptr);
  }

  node_ = std::make_shared<rclcpp::Node>("fibril_type_validator");
  babel_fish_ = std::make_unique<ros_babel_fish::BabelFish>();
}

bool ROSTypeValidator::rosTypeExists(const std::string & type_name)
{
  try {
    // CompoundMessageを作成してみる
    auto msg = babel_fish_->create_message_shared(type_name);
    return msg != nullptr;
  } catch (const std::exception & e) {
    return false;
  }
}

ValidationResult ROSTypeValidator::validateRosType(const std::string & ros_type_name)
{
  ValidationResult result;

  if (!rosTypeExists(ros_type_name)) {
    result.addError("ROS type '" + ros_type_name + "' not found");

    // 類似の型名を提案
    std::string suggestion;
    if (ros_type_name.find("geometry_msgs") != std::string::npos) {
      suggestion = "Did you mean: geometry_msgs/msg/Twist, geometry_msgs/msg/Point, etc.?";
    } else if (ros_type_name.find("std_msgs") != std::string::npos) {
      suggestion = "Did you mean: std_msgs/msg/Float32, std_msgs/msg/Bool, etc.?";
    }

    if (!suggestion.empty()) {
      result.addWarning(suggestion);
    }
  }

  return result;
}

ValidationResult ROSTypeValidator::validateRosMap(
  const std::string & ros_type_name, const std::string & field_path)
{
  ValidationResult result;

  // まずROS型の存在確認
  if (!rosTypeExists(ros_type_name)) {
    result.addError("ROS type '" + ros_type_name + "' not found");
    return result;
  }

  // フィールドパスの解決
  auto field_info = resolveFieldPath(ros_type_name, field_path);

  if (!field_info) {
    result.addError("Field path '" + field_path + "' does not exist in " + ros_type_name);

    // 利用可能なフィールドを列挙
    auto fields = getFields(ros_type_name);
    if (!fields.empty()) {
      std::ostringstream oss;
      oss << "Available fields: ";
      for (size_t i = 0; i < fields.size() && i < 5; ++i) {
        if (i > 0) oss << ", ";
        oss << fields[i].name;
      }
      if (fields.size() > 5) {
        oss << ", ...";
      }
      result.addWarning(oss.str());
    }
  }

  return result;
}

std::optional<FieldInfo> ROSTypeValidator::resolveFieldPath(
  const std::string & ros_type_name, const std::string & field_path)
{
  try {
    // CompoundMessageを作成
    auto msg = babel_fish_->create_message_shared(ros_type_name);
    if (!msg) {
      return std::nullopt;
    }

    // フィールドパスを分割（例: "linear.x" → ["linear", "x"]）
    std::vector<std::string> path_parts;
    std::istringstream ss(field_path);
    std::string part;
    while (std::getline(ss, part, '.')) {
      path_parts.push_back(part);
    }

    // 各パスを順次解決
    ros_babel_fish::Message * current_msg = msg.get();

    for (size_t i = 0; i < path_parts.size(); ++i) {
      const auto & field_name = path_parts[i];

      try {
        // operator[]でフィールドにアクセス
        ros_babel_fish::Message & field = (*current_msg)[field_name];

        // 最後のパスか確認
        if (i == path_parts.size() - 1) {
          // フィールド情報を返す
          FieldInfo info;
          info.name = field_name;
          info.type_name =
            messageTypeToString(field.type());  // 型名はCompoundMessageから取得できない場合がある
          info.is_array = field.type() == ros_babel_fish::MessageTypes::Array;
          info.array_size = 0;  // サイズは動的
          return info;
        }

        // ネストした構造体に進む
        if (field.type() == ros_babel_fish::MessageTypes::Compound) {
          current_msg = &field;
        } else {
          // プリミティブ型なのにさらにパスがある
          return std::nullopt;
        }
      } catch (const std::exception & e) {
        // フィールドが見つからない
        return std::nullopt;
      }
    }

    return std::nullopt;
  } catch (const std::exception & e) {
    RCLCPP_ERROR(node_->get_logger(), "Error resolving field path: %s", e.what());
    return std::nullopt;
  }
}

std::vector<FieldInfo> ROSTypeValidator::getFields(const std::string & ros_type_name)
{
  std::vector<FieldInfo> fields;

  try {
    // CompoundMessageを作成
    auto msg = babel_fish_->create_message_shared(ros_type_name);
    if (!msg) {
      return fields;
    }

    // CompoundMessageのkeysメソッドでフィールド名を取得
    auto compound_msg = msg->as<ros_babel_fish::CompoundMessage>();
    auto keys = compound_msg.keys();

    for (const auto & key : keys) {
      try {
        ros_babel_fish::Message & field = compound_msg[key];

        FieldInfo info;
        info.name = key;
        info.type_name = messageTypeToString(field.type());
        info.is_array = field.type() == ros_babel_fish::MessageTypes::Array;
        info.array_size = 0;
        fields.push_back(info);
      } catch (const std::exception & e) {
        // フィールドアクセスエラーは無視
        continue;
      }
    }
  } catch (const std::exception & e) {
    RCLCPP_ERROR(node_->get_logger(), "Error getting fields: %s", e.what());
  }

  return fields;
}

}  // namespace fibril

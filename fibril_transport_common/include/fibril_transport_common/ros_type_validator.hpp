#pragma once

#include <memory>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <ros_babel_fish/babel_fish.hpp>
#include <string>
#include <vector>

namespace fibril
{

// 検証結果
struct ValidationResult
{
  bool is_valid = true;
  std::string error_message;
  std::vector<std::string> warnings;

  void addError(const std::string & msg)
  {
    is_valid = false;
    error_message = msg;
  }

  void addWarning(const std::string & msg) { warnings.push_back(msg); }
};

// フィールド情報
struct FieldInfo
{
  std::string name;
  std::string type_name;
  bool is_array;
  size_t array_size;
};

// ROS型検証クラス
class ROSTypeValidator
{
public:
  ROSTypeValidator();
  ~ROSTypeValidator();

  // ROS型の存在確認
  bool rosTypeExists(const std::string & type_name);

  // ros_type属性の検証
  ValidationResult validateRosType(const std::string & ros_type_name);

  // ros_mapフィールドパスの検証
  ValidationResult validateRosMap(
    const std::string & ros_type_name, const std::string & field_path);

  // フィールドパスの解決（linear.x → 型情報）
  std::optional<FieldInfo> resolveFieldPath(
    const std::string & ros_type_name, const std::string & field_path);

  // ROS型のフィールド一覧を取得
  std::vector<FieldInfo> getFields(const std::string & ros_type_name);

  // MessageTypeを文字列に変換
  static std::string messageTypeToString(ros_babel_fish::MessageType type);

private:
  std::shared_ptr<rclcpp::Node> node_;
  std::unique_ptr<ros_babel_fish::BabelFish> babel_fish_;

  // 初期化
  void initialize();
};

}  // namespace fibril

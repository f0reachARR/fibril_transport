#include <gtest/gtest.h>

#include "fibril_transport_common/ros_type_validator.hpp"

class ROSTypeValidatorTest : public ::testing::Test
{
protected:
  void SetUp() override { validator = std::make_unique<fibril::ROSTypeValidator>(); }

  void TearDown() override { validator.reset(); }

  std::unique_ptr<fibril::ROSTypeValidator> validator;
};

TEST_F(ROSTypeValidatorTest, ValidRosType)
{
  // geometry_msgs/msg/Twistは標準ROSメッセージ
  bool exists = validator->rosTypeExists("geometry_msgs/msg/Twist");
  if (!exists) {
    GTEST_SKIP() << "geometry_msgs not available in this environment";
  }

  auto result = validator->validateRosType("geometry_msgs/msg/Twist");
  EXPECT_TRUE(result.is_valid);
}

TEST_F(ROSTypeValidatorTest, InvalidRosType)
{
  auto result = validator->validateRosType("nonexistent_pkg/msg/FakeMessage");
  EXPECT_FALSE(result.is_valid);
  EXPECT_FALSE(result.error_message.empty());
}

TEST_F(ROSTypeValidatorTest, ValidRosMap)
{
  if (!validator->rosTypeExists("geometry_msgs/msg/Twist")) {
    GTEST_SKIP() << "geometry_msgs not available";
  }

  // geometry_msgs/msg/Twistにはlinear.xフィールドが存在
  auto result = validator->validateRosMap("geometry_msgs/msg/Twist", "linear");
  EXPECT_TRUE(result.is_valid) << result.error_message;
}

TEST_F(ROSTypeValidatorTest, InvalidRosMap)
{
  if (!validator->rosTypeExists("geometry_msgs/msg/Twist")) {
    GTEST_SKIP() << "geometry_msgs not available";
  }

  // 存在しないフィールド
  auto result = validator->validateRosMap("geometry_msgs/msg/Twist", "nonexistent_field");
  EXPECT_FALSE(result.is_valid);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int result = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return result;
}

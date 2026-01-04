#include <gtest/gtest.h>

#include <rclcpp/rclcpp.hpp>

#include "fibril_transport_common/dynamic_ros_interface.hpp"

using namespace fibril_transport_common;

class DynamicRosInterfaceTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    rclcpp::init(0, nullptr);
    interface_ = std::make_unique<DynamicRosInterface>();
  }

  void TearDown() override { rclcpp::shutdown(); }

  std::unique_ptr<DynamicRosInterface> interface_;
};

// ========== Type Support Tests ==========

TEST_F(DynamicRosInterfaceTest, GetMessageTypeSupport)
{
  // Test getting type support for a standard message
  auto type_support = interface_->getMessageTypeSupport("geometry_msgs/msg/Twist");
  ASSERT_NE(type_support, nullptr);
}

TEST_F(DynamicRosInterfaceTest, GetMessageTypeSupportInvalidType)
{
  // Test error handling for invalid type
  EXPECT_THROW(
    interface_->getMessageTypeSupport("invalid_msgs/msg/NonExistent"), std::runtime_error);
}

TEST_F(DynamicRosInterfaceTest, GetServiceTypeSupport)
{
  // Test getting service type support
  auto type_support = interface_->getServiceTypeSupport("std_srvs/srv/SetBool");
  ASSERT_NE(type_support, nullptr);
}

TEST_F(DynamicRosInterfaceTest, GetServiceTypeSupportInvalidType)
{
  // Test error handling for invalid service type
  EXPECT_THROW(
    interface_->getServiceTypeSupport("invalid_srvs/srv/NonExistent"), std::runtime_error);
}

// ========== Field Path Validation Tests ==========

TEST_F(DynamicRosInterfaceTest, ValidateFieldPathValid)
{
  // Test valid field paths in geometry_msgs/msg/Twist
  EXPECT_TRUE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "linear"));
  EXPECT_TRUE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "angular"));
  EXPECT_TRUE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "linear.x"));
  EXPECT_TRUE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "linear.y"));
  EXPECT_TRUE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "linear.z"));
  EXPECT_TRUE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "angular.x"));
  EXPECT_TRUE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "angular.y"));
  EXPECT_TRUE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "angular.z"));
}

TEST_F(DynamicRosInterfaceTest, ValidateFieldPathInvalid)
{
  // Test invalid field paths
  EXPECT_FALSE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "nonexistent"));
  EXPECT_FALSE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "linear.w"));
  EXPECT_FALSE(interface_->validateFieldPath("geometry_msgs/msg/Twist", "linear.x.y"));
}

TEST_F(DynamicRosInterfaceTest, ValidateFieldPathEmpty)
{
  // Empty path should be valid (points to root)
  EXPECT_TRUE(interface_->validateFieldPath("geometry_msgs/msg/Twist", ""));
}

// ========== Field Type Info Tests ==========

TEST_F(DynamicRosInterfaceTest, GetFieldTypeInfoPrimitive)
{
  // Test getting type info for a primitive field
  auto info = interface_->getFieldTypeInfo("geometry_msgs/msg/Twist", "linear.x");

  EXPECT_EQ(info.field_name, "x");
  EXPECT_TRUE(info.isPrimitive());
  EXPECT_EQ(info.asPrimitive(), PrimitiveTypeId::Double);
}

TEST_F(DynamicRosInterfaceTest, GetFieldTypeInfoCompound)
{
  // Test getting type info for a compound field
  auto info = interface_->getFieldTypeInfo("geometry_msgs/msg/Twist", "linear");

  EXPECT_EQ(info.field_name, "linear");
  EXPECT_TRUE(info.isCompound());
  EXPECT_EQ(info.asCompound()->type_name, "geometry_msgs/msg/Vector3");
}

TEST_F(DynamicRosInterfaceTest, GetFieldTypeInfoInvalidPath)
{
  // Test error handling for invalid field path
  EXPECT_THROW(
    interface_->getFieldTypeInfo("geometry_msgs/msg/Twist", "nonexistent"), std::runtime_error);
}

// ========== Field List Tests (LSP Support) ==========

TEST_F(DynamicRosInterfaceTest, GetFieldListTopLevel)
{
  // Test getting top-level fields
  auto fields = interface_->getFieldList("geometry_msgs/msg/Twist", "");

  ASSERT_EQ(fields.size(), 2);

  // Check that we have linear and angular
  bool has_linear = false;
  bool has_angular = false;

  for (const auto & field : fields) {
    if (field.field_name == "linear") {
      has_linear = true;
      EXPECT_TRUE(field.isCompound());
      EXPECT_EQ(field.asCompound()->type_name, "geometry_msgs/msg/Vector3");
    } else if (field.field_name == "angular") {
      has_angular = true;
      EXPECT_TRUE(field.isCompound());
      EXPECT_EQ(field.asCompound()->type_name, "geometry_msgs/msg/Vector3");
    }
  }

  EXPECT_TRUE(has_linear);
  EXPECT_TRUE(has_angular);
}

TEST_F(DynamicRosInterfaceTest, GetFieldListNested)
{
  // Test getting nested fields (linear -> x, y, z)
  auto fields = interface_->getFieldList("geometry_msgs/msg/Twist", "linear");

  ASSERT_EQ(fields.size(), 3);

  // Check that we have x, y, z
  bool has_x = false, has_y = false, has_z = false;

  for (const auto & field : fields) {
    EXPECT_TRUE(field.isPrimitive());
    EXPECT_EQ(field.asPrimitive(), PrimitiveTypeId::Double);

    if (field.field_name == "x")
      has_x = true;
    else if (field.field_name == "y")
      has_y = true;
    else if (field.field_name == "z")
      has_z = true;
  }

  EXPECT_TRUE(has_x);
  EXPECT_TRUE(has_y);
  EXPECT_TRUE(has_z);
}

TEST_F(DynamicRosInterfaceTest, GetFieldListInvalidPath)
{
  // Test error for invalid path (pointing to primitive)
  EXPECT_THROW(interface_->getFieldList("geometry_msgs/msg/Twist", "linear.x"), std::runtime_error);
}

// ========== Message Creation Tests ==========

TEST_F(DynamicRosInterfaceTest, CreateMessage)
{
  // Test creating a message
  auto msg = interface_->createMessage("geometry_msgs/msg/Twist");

  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(msg->name(), "geometry_msgs/msg/Twist");
}

TEST_F(DynamicRosInterfaceTest, CreateMessageInvalidType)
{
  // Test error handling for invalid type
  EXPECT_THROW(interface_->createMessage("invalid_msgs/msg/NonExistent"), std::runtime_error);
}

// ========== Field Value Set/Get Tests ==========

TEST_F(DynamicRosInterfaceTest, SetGetFieldValueFloat)
{
  auto msg = interface_->createMessage("geometry_msgs/msg/Twist");

  // Set a value
  interface_->setFieldValue<double>(*msg, "linear.x", 1.5);

  // Get it back
  double value = interface_->getFieldValue<double>(*msg, "linear.x");
  EXPECT_DOUBLE_EQ(value, 1.5);
}

TEST_F(DynamicRosInterfaceTest, SetGetFieldValueInt)
{
  auto msg = interface_->createMessage("std_msgs/msg/Int32");

  // Set a value
  interface_->setFieldValue<int32_t>(*msg, "data", 42);

  // Get it back
  int32_t value = interface_->getFieldValue<int32_t>(*msg, "data");
  EXPECT_EQ(value, 42);
}

TEST_F(DynamicRosInterfaceTest, SetGetFieldValueBool)
{
  auto msg = interface_->createMessage("std_msgs/msg/Bool");

  // Set a value
  interface_->setFieldValue<bool>(*msg, "data", true);

  // Get it back
  bool value = interface_->getFieldValue<bool>(*msg, "data");
  EXPECT_TRUE(value);
}

TEST_F(DynamicRosInterfaceTest, SetFieldValueMultipleFields)
{
  auto msg = interface_->createMessage("geometry_msgs/msg/Twist");

  // Set multiple values
  interface_->setFieldValue<double>(*msg, "linear.x", 1.0);
  interface_->setFieldValue<double>(*msg, "linear.y", 2.0);
  interface_->setFieldValue<double>(*msg, "linear.z", 3.0);
  interface_->setFieldValue<double>(*msg, "angular.x", 0.1);
  interface_->setFieldValue<double>(*msg, "angular.y", 0.2);
  interface_->setFieldValue<double>(*msg, "angular.z", 0.3);

  // Verify all values
  EXPECT_DOUBLE_EQ(interface_->getFieldValue<double>(*msg, "linear.x"), 1.0);
  EXPECT_DOUBLE_EQ(interface_->getFieldValue<double>(*msg, "linear.y"), 2.0);
  EXPECT_DOUBLE_EQ(interface_->getFieldValue<double>(*msg, "linear.z"), 3.0);
  EXPECT_DOUBLE_EQ(interface_->getFieldValue<double>(*msg, "angular.x"), 0.1);
  EXPECT_DOUBLE_EQ(interface_->getFieldValue<double>(*msg, "angular.y"), 0.2);
  EXPECT_DOUBLE_EQ(interface_->getFieldValue<double>(*msg, "angular.z"), 0.3);
}

// ========== Service Tests ==========

TEST_F(DynamicRosInterfaceTest, CreateServiceRequest)
{
  // Test creating a service request
  auto type_support = interface_->getServiceTypeSupport("std_srvs/srv/SetBool");
  auto request = interface_->createMessage(type_support->request());

  ASSERT_NE(request, nullptr);

  // Set request data
  interface_->setFieldValue<bool>(*request, "data", true);

  EXPECT_THROW(interface_->setFieldValue<bool>(*request, "nonexistent", true), std::runtime_error);

  // Verify
  bool value = interface_->getFieldValue<bool>(*request, "data");
  EXPECT_TRUE(value);
}

TEST_F(DynamicRosInterfaceTest, CreateServiceResponse)
{
  // Test creating a service response
  auto type_support = interface_->getServiceTypeSupport("std_srvs/srv/SetBool");
  auto response = interface_->createMessage(type_support->response());

  ASSERT_NE(response, nullptr);

  // Set response data
  interface_->setFieldValue<bool>(*response, "success", true);

  EXPECT_THROW(interface_->setFieldValue<bool>(*response, "nonexistent", true), std::runtime_error);

  // Verify
  bool value = interface_->getFieldValue<bool>(*response, "success");
  EXPECT_TRUE(value);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

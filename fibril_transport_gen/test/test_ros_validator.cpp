#include <gtest/gtest.h>

#include "parser/parser.hpp"
#include "validator/validator.hpp"

class RosValidatorTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    parser = std::make_unique<fibril::Parser>();
    validator = std::make_unique<fibril::Validator>();

    // Enable ROS validation
    try {
      validator->setRosValidationEnabled(true);
    } catch (const std::exception & e) {
      GTEST_SKIP() << "ROS validation not available: " << e.what();
    }
  }

  void TearDown() override
  {
    parser.reset();
    validator.reset();
  }

  std::unique_ptr<fibril::Parser> parser;
  std::unique_ptr<fibril::Validator> validator;
};

// ========== ROS Type Validation Tests ==========

TEST_F(RosValidatorTest, ValidRosType)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

#[ros_type(geometry_msgs/msg/Twist)]
struct Twist2D {
    float v;
    float w;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_TRUE(valid) << "Valid ROS type should pass validation";
  EXPECT_TRUE(validator->getErrors().empty());
}

TEST_F(RosValidatorTest, InvalidRosType)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

#[ros_type(nonexistent_msgs/msg/Fake)]
struct BadStruct {
    float value;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_FALSE(valid) << "Nonexistent ROS type should fail validation";
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("Invalid ROS message type"), std::string::npos);
}

// ========== ROS Map Validation Tests ==========

TEST_F(RosValidatorTest, ValidRosMapAttributes)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

#[ros_type(geometry_msgs/msg/Twist)]
struct Twist2D {
    #[ros_map(linear.x)]
    float v;
    
    #[ros_map(angular.z)]
    float w;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_TRUE(valid) << "Valid ros_map attributes should pass validation";
  EXPECT_TRUE(validator->getErrors().empty());
}

TEST_F(RosValidatorTest, InvalidRosMapFieldPath)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

#[ros_type(geometry_msgs/msg/Twist)]
struct BadMapping {
    #[ros_map(nonexistent.field)]
    float value;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_FALSE(valid) << "Invalid field path should fail validation";
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("Invalid field path"), std::string::npos);
}

// ========== ROS Pub/Sub Validation Tests ==========

TEST_F(RosValidatorTest, ValidRosPubSubType)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

#[ros_type(geometry_msgs/msg/Twist)]
struct Twist2D {
    #[ros_map(linear.x)]
    float v;
    
    #[ros_map(angular.z)]
    float w;
}

node TestNode {
    #[ros("~/twist_pub")]
    pub twist_pub(Twist2D);
    #[ros("~/twist_sub")]
    sub twist_sub(Twist2D);
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_TRUE(valid) << "Valid ROS pub/sub type should pass validation";
  EXPECT_TRUE(validator->getErrors().empty());
}

// ========== ROS Service Validation Tests ==========

TEST_F(RosValidatorTest, ValidRosServiceType)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct SetBoolRequest {
    #[ros_map(data)]
    bool data;
}

struct SetBoolResponse {
    #[ros_map(success)]
    bool success;
}

node TestNode {
    #[ros("~/enable_motor")]
    #[ros_service(std_srvs/srv/SetBool)]
    service enable_motor(SetBoolRequest) -> SetBoolResponse;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);

  // Debug output
  if (!valid) {
    for (const auto & error : validator->getErrors()) {
      std::cerr << "Validation error: " << error.format() << std::endl;
    }
  }

  EXPECT_TRUE(valid) << "Valid ROS service type should pass validation";
  EXPECT_TRUE(validator->getErrors().empty());
}

TEST_F(RosValidatorTest, InvalidRosServiceType)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct DummyRequest {
    bool value;
}

struct DummyResponse {
    bool result;
}

node TestNode {
    #[ros_service(nonexistent_srvs/srv/FakeService)]
    service bad_service(DummyRequest) -> DummyResponse;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_FALSE(valid) << "Nonexistent ROS service type should fail validation";
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("Invalid ROS service type"), std::string::npos);
}

// ========== Service Field Mapping Validation Tests ==========

TEST_F(RosValidatorTest, ValidServiceFieldMapping)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct SetBoolRequest {
    #[ros_map(data)]
    bool data;
}

struct SetBoolResponse {
    #[ros_map(success)]
    bool success;
    
    #[ros_map(message)]
    uint8 msg[256];
}

node TestNode {
    #[ros("~/enable_motor")]
    #[ros_service(std_srvs/srv/SetBool)]
    service enable_motor(SetBoolRequest) -> SetBoolResponse;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_TRUE(valid) << "Valid service field mapping should pass validation";
  EXPECT_TRUE(validator->getErrors().empty());
}

TEST_F(RosValidatorTest, InvalidServiceRequestFieldMapping)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct InvalidSetBoolRequest {
    #[ros_map(nonexistent_field)]
    bool value;
}

struct SetBoolResponse {
    #[ros_map(success)]
    bool success;
}

node TestNode {
    #[ros("~/bad_mapping")]
    #[ros_service(std_srvs/srv/SetBool)]
    service bad_mapping(InvalidSetBoolRequest) -> SetBoolResponse;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_FALSE(valid) << "Invalid service request field mapping should fail validation";
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("in service request type"), std::string::npos);
}

TEST_F(RosValidatorTest, InvalidServiceResponseFieldMapping)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct SetBoolRequest {
    #[ros_map(data)]
    bool data;
}

struct InvalidSetBoolResponse {
    #[ros_map(nonexistent_field)]
    bool result;
}

node TestNode {
    #[ros("~/bad_mapping")]
    #[ros_service(std_srvs/srv/SetBool)]
    service bad_mapping(SetBoolRequest) -> InvalidSetBoolResponse;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_FALSE(valid) << "Invalid service response field mapping should fail validation";
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("in service response type"), std::string::npos);
}

TEST_F(RosValidatorTest, ServiceFieldWithoutRosMap)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct SetBoolRequest {
    bool wrong_field_name;
}

struct SetBoolResponse {
    #[ros_map(success)]
    bool success;
}

node TestNode {
    #[ros("~/test_service")]
    #[ros_service(std_srvs/srv/SetBool)]
    service test_service(SetBoolRequest) -> SetBoolResponse;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_FALSE(valid) << "Service field without ros_map and wrong name should fail validation";
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("Field 'wrong_field_name'"), std::string::npos);
}

// ========== ROS Validation Enable/Disable Tests ==========

TEST_F(RosValidatorTest, ValidationDisabled)
{
  // Disable ROS validation
  validator->setRosValidationEnabled(false);

  const std::string source = R"(
syntax = "fibril v2";
package test;

#[ros_type(nonexistent_msgs/msg/Fake)]
struct BadStruct {
    float value;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_TRUE(valid) << "Invalid ROS types should pass when ROS validation is disabled";
  EXPECT_TRUE(validator->getErrors().empty());
}

// ========== Complex Validation Tests ==========

TEST_F(RosValidatorTest, MultipleStructsAndServices)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

#[ros_type(geometry_msgs/msg/Twist)]
struct Twist2D {
    #[ros_map(linear.x)]
    float v;
    
    #[ros_map(angular.z)]
    float w;
}

struct SetBoolRequest {
    #[ros_map(data)]
    bool data;
}

struct SetBoolResponse {
    #[ros_map(success)]
    bool success;
}

node TestNode {
    #[ros("~/enable_motor")]
    #[ros_service(std_srvs/srv/SetBool)]
    service enable_motor(SetBoolRequest) -> SetBoolResponse;
    
    #[ros("~/velocity")]
    pub Twist2D velocity;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_TRUE(valid) << "Complex valid DSL should pass validation";
  EXPECT_TRUE(validator->getErrors().empty());
}

TEST_F(RosValidatorTest, MultipleErrors)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

#[ros_type(nonexistent_msgs/msg/Fake1)]
struct BadStruct1 {
    float value;
}

#[ros_type(nonexistent_msgs/msg/Fake2)]
struct BadStruct2 {
    float value;
}

struct BadRequest {
    #[ros_map(nonexistent)]
    bool value;
}

struct BadResponse {
    bool result;
}

node TestNode {
    #[ros("~/bad_service")]
    #[ros_service(std_srvs/srv/SetBool)]
    service bad_service(BadRequest) -> BadResponse;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_FALSE(valid) << "Multiple invalid ROS types should fail validation";
  EXPECT_GE(validator->getErrors().size(), 2) << "Should report multiple errors";
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

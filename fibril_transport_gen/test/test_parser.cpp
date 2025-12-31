#include <gtest/gtest.h>

#include "parser/parser.hpp"

class ParserTest : public ::testing::Test
{
protected:
  void SetUp() override { parser = std::make_unique<fibril::Parser>(); }

  void TearDown() override { parser.reset(); }

  std::unique_ptr<fibril::Parser> parser;
};

TEST_F(ParserTest, BasicSyntaxAndPackage)
{
  const std::string test_content = R"(
syntax = "fibril v2";
package test.basic;
)";

  auto ast = parser->parseFromString(test_content);

  EXPECT_EQ(ast.syntax.version, "fibril v2");
  EXPECT_EQ(ast.package.name, "test.basic");
  EXPECT_TRUE(parser->getErrors().empty());
}

TEST_F(ParserTest, SimpleStruct)
{
  const std::string test_content = R"(
syntax = "fibril v2";
package test;

struct Vector3 {
    float x;
    float y;
    float z;
}
)";

  auto ast = parser->parseFromString(test_content);

  ASSERT_EQ(ast.structs.size(), 1);
  EXPECT_EQ(ast.structs[0].name, "Vector3");
  ASSERT_EQ(ast.structs[0].fields.size(), 3);

  EXPECT_EQ(ast.structs[0].fields[0].name, "x");
  EXPECT_TRUE(ast.structs[0].fields[0].type.isPrimitive());
  EXPECT_EQ(ast.structs[0].fields[0].type.asPrimitive(), fibril::PrimitiveType::Float);

  EXPECT_EQ(ast.structs[0].fields[1].name, "y");
  EXPECT_EQ(ast.structs[0].fields[2].name, "z");

  EXPECT_TRUE(parser->getErrors().empty());
}

TEST_F(ParserTest, StructWithAttributes)
{
  const std::string test_content = R"(
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

  auto ast = parser->parseFromString(test_content);

  ASSERT_EQ(ast.structs.size(), 1);
  EXPECT_EQ(ast.structs[0].name, "Twist2D");

  // Struct属性
  ASSERT_EQ(ast.structs[0].attributes.size(), 1);
  EXPECT_EQ(ast.structs[0].attributes[0].name, "ros_type");
  ASSERT_EQ(ast.structs[0].attributes[0].arguments.size(), 1);
  EXPECT_TRUE(std::holds_alternative<std::string>(ast.structs[0].attributes[0].arguments[0]));
  auto ros_type_arg = ast.structs[0].attributes[0].getStringArg(0);
  ASSERT_TRUE(ros_type_arg.has_value());
  EXPECT_EQ(ros_type_arg.value(), "geometry_msgs/msg/Twist");

  // フィールド属性
  ASSERT_EQ(ast.structs[0].fields.size(), 2);
  EXPECT_EQ(ast.structs[0].fields[0].name, "v");
  ASSERT_EQ(ast.structs[0].fields[0].attributes.size(), 1);
  EXPECT_EQ(ast.structs[0].fields[0].attributes[0].name, "ros_map");

  EXPECT_TRUE(parser->getErrors().empty());
}

TEST_F(ParserTest, NodeWithPubSub)
{
  const std::string test_content = R"(
syntax = "fibril v2";
package test;

struct Vector3 {
    float x;
    float y;
    float z;
}

node TestNode {
    #[ros("/position")]
    pub Vector3 pos;
    
    #[ros("/velocity")]
    sub Vector3 vel;
}
)";

  auto ast = parser->parseFromString(test_content);

  ASSERT_EQ(ast.nodes.size(), 1);
  EXPECT_EQ(
    ast

      .nodes[0]
      .name,
    "TestNode");

  ASSERT_EQ(ast.nodes[0].ports.size(), 2);

  // Pub port
  ASSERT_TRUE(std::holds_alternative<fibril::PubPort>(ast.nodes[0].ports[0]));
  const auto & pub = std::get<fibril::PubPort>(ast.nodes[0].ports[0]);
  EXPECT_EQ(pub.name, "pos");
  EXPECT_TRUE(pub.data_type.isStruct());
  EXPECT_EQ(pub.data_type.asStruct().name, "Vector3");

  // Sub port
  ASSERT_TRUE(std::holds_alternative<fibril::SubPort>(ast.nodes[0].ports[1]));
  const auto & sub = std::get<fibril::SubPort>(ast.nodes[0].ports[1]);
  EXPECT_EQ(sub.name, "vel");
  EXPECT_TRUE(sub.data_type.isStruct());
  EXPECT_EQ(sub.data_type.asStruct().name, "Vector3");

  EXPECT_TRUE(parser->getErrors().empty());
}

TEST_F(ParserTest, ArrayFields)
{
  const std::string test_content = R"(
syntax = "fibril v2";
package test;

struct IMUData {
    float gyro[3];
    float accel[3];
}
)";

  auto ast = parser->parseFromString(test_content);

  ASSERT_EQ(ast.structs.size(), 1);
  ASSERT_EQ(ast.structs[0].fields.size(), 2);

  // 配列フィールド
  EXPECT_EQ(ast.structs[0].fields[0].name, "gyro");
  EXPECT_TRUE(ast.structs[0].fields[0].type.isArray());
  const auto & array_type = ast.structs[0].fields[0].type.asArray();
  EXPECT_EQ(array_type.size, 3);
  EXPECT_TRUE(array_type.element_type->isPrimitive());
  EXPECT_EQ(array_type.element_type->asPrimitive(), fibril::PrimitiveType::Float);

  EXPECT_TRUE(parser->getErrors().empty());
}

TEST_F(ParserTest, ServicePort)
{
  const std::string test_content = R"(
syntax = "fibril v2";
package test;

struct Request {
    bool enable;
}

struct Response {
    bool success;
}

node Device {
    #[ros_service(std_srvs/srv/SetBool)]
    service enable_motor(Request) -> Response;
}
)";

  auto ast = parser->parseFromString(test_content);

  ASSERT_EQ(ast.nodes.size(), 1);
  ASSERT_EQ(ast.nodes[0].ports.size(), 1);

  // Service port
  ASSERT_TRUE(std::holds_alternative<fibril::ServicePort>(ast.nodes[0].ports[0]));
  const auto & svc = std::get<fibril::ServicePort>(ast.nodes[0].ports[0]);
  EXPECT_EQ(svc.name, "enable_motor");

  EXPECT_TRUE(svc.request_type.isStruct());
  EXPECT_EQ(svc.request_type.asStruct().name, "Request");

  EXPECT_TRUE(svc.response_type.isStruct());
  EXPECT_EQ(svc.response_type.asStruct().name, "Response");

  EXPECT_TRUE(parser->getErrors().empty());
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

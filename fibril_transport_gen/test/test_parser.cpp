#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include "parser/parser.hpp"

class ParserTest : public ::testing::Test
{
protected:
  void SetUp() override { parser = std::make_unique<fibril::Parser>(); }

  void TearDown() override { parser.reset(); }

  std::unique_ptr<fibril::Parser> parser;

  // テスト用のFibrilファイルを作成
  void createTestFile(const std::string & filename, const std::string & content)
  {
    std::ofstream file(filename);
    file << content;
    file.close();
  }

  void removeTestFile(const std::string & filename) { std::remove(filename.c_str()); }
};

TEST_F(ParserTest, BasicSyntaxAndPackage)
{
  const std::string test_content = R"(
syntax = "fibril v2";
package test.basic;
)";

  const std::string test_file = "/tmp/test_basic.fibril";
  createTestFile(test_file, test_content);

  auto ast = parser->parse(test_file);

  EXPECT_EQ(ast.syntax.version, "fibril v2");
  EXPECT_EQ(ast.package.name, "test.basic");
  EXPECT_TRUE(parser->getErrors().empty());

  removeTestFile(test_file);
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

  const std::string test_file = "/tmp/test_struct.fibril";
  createTestFile(test_file, test_content);

  auto ast = parser->parse(test_file);

  ASSERT_EQ(ast.structs.size(), 1);
  EXPECT_EQ(ast.structs[0].name, "Vector3");
  ASSERT_EQ(ast.structs[0].fields.size(), 3);

  EXPECT_EQ(ast.structs[0].fields[0].name, "x");
  EXPECT_EQ(ast.structs[0].fields[0].type.kind, fibril::Type::Kind::Primitive);
  EXPECT_EQ(ast.structs[0].fields[0].type.primitive_type, fibril::PrimitiveType::Float);

  EXPECT_EQ(ast.structs[0].fields[1].name, "y");
  EXPECT_EQ(ast.structs[0].fields[2].name, "z");

  EXPECT_TRUE(parser->getErrors().empty());

  removeTestFile(test_file);
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

  const std::string test_file = "/tmp/test_struct_attr.fibril";
  createTestFile(test_file, test_content);

  auto ast = parser->parse(test_file);

  ASSERT_EQ(ast.structs.size(), 1);
  EXPECT_EQ(ast.structs[0].name, "Twist2D");

  // Struct属性
  ASSERT_EQ(ast.structs[0].attributes.size(), 1);
  EXPECT_EQ(ast.structs[0].attributes[0].name, "ros_type");
  ASSERT_EQ(ast.structs[0].attributes[0].arguments.size(), 1);
  EXPECT_EQ(
    ast.structs[0].attributes[0].arguments[0].kind, fibril::AttributeValue::Kind::Identifier);
  auto ros_type_arg = ast.structs[0].attributes[0].getStringArg(0);
  ASSERT_TRUE(ros_type_arg.has_value());
  EXPECT_EQ(ros_type_arg.value(), "geometry_msgs/msg/Twist");

  // フィールド属性
  ASSERT_EQ(ast.structs[0].fields.size(), 2);
  EXPECT_EQ(ast.structs[0].fields[0].name, "v");
  ASSERT_EQ(ast.structs[0].fields[0].attributes.size(), 1);
  EXPECT_EQ(ast.structs[0].fields[0].attributes[0].name, "ros_map");

  EXPECT_TRUE(parser->getErrors().empty());

  removeTestFile(test_file);
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

  const std::string test_file = "/tmp/test_node.fibril";
  createTestFile(test_file, test_content);

  auto ast = parser->parse(test_file);

  ASSERT_EQ(ast.nodes.size(), 1);
  EXPECT_EQ(ast.nodes[0].name, "TestNode");

  ASSERT_EQ(ast.nodes[0].ports.size(), 2);

  // Pub port
  EXPECT_EQ(ast.nodes[0].ports[0].name, "pos");
  EXPECT_EQ(ast.nodes[0].ports[0].direction, fibril::Port::Direction::Pub);
  EXPECT_EQ(ast.nodes[0].ports[0].data_type.kind, fibril::Type::Kind::Struct);
  EXPECT_EQ(ast.nodes[0].ports[0].data_type.struct_name, "Vector3");

  // Sub port
  EXPECT_EQ(ast.nodes[0].ports[1].name, "vel");
  EXPECT_EQ(ast.nodes[0].ports[1].direction, fibril::Port::Direction::Sub);

  EXPECT_TRUE(parser->getErrors().empty());

  removeTestFile(test_file);
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

  const std::string test_file = "/tmp/test_array.fibril";
  createTestFile(test_file, test_content);

  auto ast = parser->parse(test_file);

  ASSERT_EQ(ast.structs.size(), 1);
  ASSERT_EQ(ast.structs[0].fields.size(), 2);

  // 配列フィールド
  EXPECT_EQ(ast.structs[0].fields[0].name, "gyro");
  EXPECT_EQ(ast.structs[0].fields[0].type.kind, fibril::Type::Kind::Array);
  EXPECT_EQ(ast.structs[0].fields[0].type.array_size, 3);
  EXPECT_EQ(ast.structs[0].fields[0].type.element_type->kind, fibril::Type::Kind::Primitive);
  EXPECT_EQ(
    ast.structs[0].fields[0].type.element_type->primitive_type, fibril::PrimitiveType::Float);

  EXPECT_TRUE(parser->getErrors().empty());

  removeTestFile(test_file);
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

  const std::string test_file = "/tmp/test_service.fibril";
  createTestFile(test_file, test_content);

  auto ast = parser->parse(test_file);

  ASSERT_EQ(ast.nodes.size(), 1);
  ASSERT_EQ(ast.nodes[0].ports.size(), 1);

  auto & port = ast.nodes[0].ports[0];
  EXPECT_EQ(port.name, "enable_motor");
  EXPECT_EQ(port.direction, fibril::Port::Direction::Service);

  ASSERT_TRUE(port.request_type.has_value());
  EXPECT_EQ(port.request_type->kind, fibril::Type::Kind::Struct);
  EXPECT_EQ(port.request_type->struct_name, "Request");

  ASSERT_TRUE(port.response_type.has_value());
  EXPECT_EQ(port.response_type->kind, fibril::Type::Kind::Struct);
  EXPECT_EQ(port.response_type->struct_name, "Response");

  EXPECT_TRUE(parser->getErrors().empty());

  removeTestFile(test_file);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

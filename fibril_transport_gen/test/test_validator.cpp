#include <gtest/gtest.h>

#include "parser/parser.hpp"
#include "validator/validator.hpp"

class ValidatorTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    parser = std::make_unique<fibril::Parser>();
    validator = std::make_unique<fibril::Validator>();
  }

  void TearDown() override
  {
    parser.reset();
    validator.reset();
  }

  std::unique_ptr<fibril::Parser> parser;
  std::unique_ptr<fibril::Validator> validator;
};

TEST_F(ValidatorTest, ValidStruct)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct Vector3 {
    float x;
    float y;
    float z;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(parser->getErrors().empty());

  bool valid = validator->validate(ast);
  EXPECT_TRUE(valid);
  EXPECT_TRUE(validator->getErrors().empty());
}

TEST_F(ValidatorTest, UnknownTypeReference)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct Container {
    UnknownType data;
}
)";

  auto ast = parser->parseFromString(source);
  bool valid = validator->validate(ast);

  EXPECT_FALSE(valid);
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("Unknown type"), std::string::npos);
}

TEST_F(ValidatorTest, DuplicateStructDefinition)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct Vector3 {
    float x;
}

struct Vector3 {
    float y;
}
)";

  auto ast = parser->parseFromString(source);
  bool valid = validator->validate(ast);

  EXPECT_FALSE(valid);
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("Duplicate"), std::string::npos);
}

TEST_F(ValidatorTest, DuplicateFieldName)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct BadStruct {
    float x;
    float x;
}
)";

  auto ast = parser->parseFromString(source);
  bool valid = validator->validate(ast);

  EXPECT_FALSE(valid);
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("Duplicate field"), std::string::npos);
}

TEST_F(ValidatorTest, CircularReference)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct A {
    B b_field;
}

struct B {
    A a_field;
}
)";

  auto ast = parser->parseFromString(source);
  bool valid = validator->validate(ast);

  EXPECT_FALSE(valid);
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("Circular reference"), std::string::npos);
}

TEST_F(ValidatorTest, ValidNodeWithPorts)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct Vector3 {
    float x;
    float y;
    float z;
}

node TestNode {
    pub Vector3 position;
    sub Vector3 velocity;
}
)";

  auto ast = parser->parseFromString(source);
  bool valid = validator->validate(ast);

  EXPECT_TRUE(valid);
  EXPECT_TRUE(validator->getErrors().empty());
}

TEST_F(ValidatorTest, DuplicatePortName)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct Data {
    float value;
}

node BadNode {
    pub Data data;
    sub Data data;
}
)";

  auto ast = parser->parseFromString(source);
  bool valid = validator->validate(ast);

  EXPECT_FALSE(valid);
  ASSERT_FALSE(validator->getErrors().empty());
  EXPECT_NE(validator->getErrors()[0].message.find("Duplicate port"), std::string::npos);
}

TEST_F(ValidatorTest, ServicePortValidation)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct Request {
    bool enable;
}

struct Response {
    bool success;
}

node ServiceNode {
    service enable_motor(Request) -> Response;
}
)";

  auto ast = parser->parseFromString(source);
  bool valid = validator->validate(ast);

  EXPECT_TRUE(valid);
  EXPECT_TRUE(validator->getErrors().empty());
}

TEST_F(ValidatorTest, TypeSizeCalculation)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct Simple {
    uint8 a;
    uint16 b;
    uint32 c;
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(validator->validate(ast));

  fibril::TypeSizeCalculator calc;

  // プリミティブ型のサイズ
  EXPECT_EQ(calc.getPrimitiveSize(fibril::PrimitiveType::Int8), 1);
  EXPECT_EQ(calc.getPrimitiveSize(fibril::PrimitiveType::Int16), 2);
  EXPECT_EQ(calc.getPrimitiveSize(fibril::PrimitiveType::Int32), 4);
  EXPECT_EQ(calc.getPrimitiveSize(fibril::PrimitiveType::Float), 4);
  EXPECT_EQ(calc.getPrimitiveSize(fibril::PrimitiveType::Double), 8);

  // 構造体のサイズ（アライメント考慮）
  const auto * simple_struct = ast.findStruct("Simple");
  ASSERT_NE(simple_struct, nullptr);
  size_t size = calc.calculateStructSize(*simple_struct, ast);
  // uint8(1) + padding(1) + uint16(2) + uint32(4) = 8 bytes
  EXPECT_EQ(size, 8);
}

TEST_F(ValidatorTest, ArrayTypeSize)
{
  const std::string source = R"(
syntax = "fibril v2";
package test;

struct ArrayStruct {
    float data[10];
}
)";

  auto ast = parser->parseFromString(source);
  ASSERT_TRUE(validator->validate(ast));

  fibril::TypeSizeCalculator calc;
  const auto * array_struct = ast.findStruct("ArrayStruct");
  ASSERT_NE(array_struct, nullptr);

  size_t size = calc.calculateStructSize(*array_struct, ast);
  // float[10] = 4 * 10 = 40 bytes
  EXPECT_EQ(size, 40);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#include <gtest/gtest.h>

#include "fibril_transport_common/type_system.hpp"

using namespace fibril_transport_common;

// ========================================
// PrimitiveType Tests
// ========================================

TEST(TypeSystemTest, PrimitiveTypeSize)
{
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::Bool), 1);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::Int8), 1);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::UInt8), 1);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::Int16), 2);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::UInt16), 2);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::Int32), 4);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::UInt32), 4);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::Int64), 8);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::UInt64), 8);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::Float), 4);
  EXPECT_EQ(getPrimitiveTypeSize(PrimitiveType::Double), 8);
}

// ========================================
// TypeDescriptor Tests
// ========================================

TEST(TypeSystemTest, PrimitiveTypeDescriptor)
{
  auto type_desc = TypeDescriptor::makePrimitive(PrimitiveType::Float);

  EXPECT_EQ(type_desc.kind(), TypeDescriptor::Kind::Primitive);
  EXPECT_TRUE(type_desc.isPrimitive());
  EXPECT_FALSE(type_desc.isStruct());
  EXPECT_FALSE(type_desc.isArray());
  EXPECT_EQ(type_desc.asPrimitive(), PrimitiveType::Float);
}

TEST(TypeSystemTest, StructTypeDescriptor)
{
  auto type_desc = TypeDescriptor::makeStruct("Twist2D");

  EXPECT_EQ(type_desc.kind(), TypeDescriptor::Kind::Struct);
  EXPECT_FALSE(type_desc.isPrimitive());
  EXPECT_TRUE(type_desc.isStruct());
  EXPECT_FALSE(type_desc.isArray());
  EXPECT_EQ(type_desc.asStructName(), "Twist2D");
}

TEST(TypeSystemTest, ArrayTypeDescriptor)
{
  auto element_type = TypeDescriptor::makePrimitive(PrimitiveType::Int32);
  auto array_type = TypeDescriptor::makeArray(std::move(element_type), 10);

  EXPECT_EQ(array_type.kind(), TypeDescriptor::Kind::Array);
  EXPECT_FALSE(array_type.isPrimitive());
  EXPECT_FALSE(array_type.isStruct());
  EXPECT_TRUE(array_type.isArray());
  EXPECT_EQ(array_type.arraySize(), 10);
  EXPECT_TRUE(array_type.arrayElement().isPrimitive());
  EXPECT_EQ(array_type.arrayElement().asPrimitive(), PrimitiveType::Int32);
}

TEST(TypeSystemTest, TypeDescriptorCopy)
{
  auto original = TypeDescriptor::makeStruct("TestStruct");
  auto copied = original;

  EXPECT_EQ(copied.kind(), TypeDescriptor::Kind::Struct);
  EXPECT_EQ(copied.asStructName(), "TestStruct");
  EXPECT_EQ(original.asStructName(), "TestStruct");
}

TEST(TypeSystemTest, TypeDescriptorMove)
{
  auto original = TypeDescriptor::makeStruct("TestStruct");
  auto moved = std::move(original);

  EXPECT_EQ(moved.kind(), TypeDescriptor::Kind::Struct);
  EXPECT_EQ(moved.asStructName(), "TestStruct");
}

TEST(TypeSystemTest, TypeDescriptorWrongAccessThrows)
{
  auto type_desc = TypeDescriptor::makePrimitive(PrimitiveType::Float);

  EXPECT_THROW(type_desc.asStructName(), std::runtime_error);
  EXPECT_THROW(type_desc.arraySize(), std::runtime_error);
}

// ========================================
// FieldDescriptor Tests
// ========================================

TEST(TypeSystemTest, FieldDescriptor)
{
  FieldDescriptor field("velocity", TypeDescriptor::makePrimitive(PrimitiveType::Float));

  EXPECT_EQ(field.name, "velocity");
  EXPECT_TRUE(field.type.isPrimitive());
}

// ========================================
// StructDescriptor Tests
// ========================================

TEST(TypeSystemTest, StructDescriptor)
{
  StructDescriptor struct_desc("Twist2D");

  FieldDescriptor field1("linear_x", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  struct_desc.fields.push_back(std::move(field1));

  FieldDescriptor field2("angular_z", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  struct_desc.fields.push_back(std::move(field2));

  EXPECT_EQ(struct_desc.name, "Twist2D");
  EXPECT_EQ(struct_desc.fields.size(), 2);

  auto * found = struct_desc.findField("linear_x");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->name, "linear_x");

  auto * not_found = struct_desc.findField("does_not_exist");
  EXPECT_EQ(not_found, nullptr);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

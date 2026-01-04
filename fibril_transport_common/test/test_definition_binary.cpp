#include <gtest/gtest.h>

#include "fibril_transport_common/definition_binary.hpp"

using namespace fibril_transport_common;

// ============================================================================
// DefinitionBinaryBuilder Tests
// ============================================================================

TEST(DefinitionBinaryBuilderTest, BuildEmptyDefinition)
{
  DefinitionBinaryBuilder builder;
  builder.begin("TestNode");

  auto data = builder.build();

  // Expected: magic(4) + version(2) + name_len(1) + name(8) + port_count(1) = 16 bytes
  ASSERT_EQ(data.size(), 16);

  // Verify magic number (little-endian)
  EXPECT_EQ(data[0], 0x4C);  // 'L'
  EXPECT_EQ(data[1], 0x52);  // 'R'
  EXPECT_EQ(data[2], 0x42);  // 'B'
  EXPECT_EQ(data[3], 0x46);  // 'F'

  // Verify version (0x0200)
  EXPECT_EQ(data[4], 0x00);
  EXPECT_EQ(data[5], 0x02);

  // Verify node name length
  EXPECT_EQ(data[6], 8);  // "TestNode" is 8 characters

  // Verify node name
  std::string node_name(data.begin() + 7, data.begin() + 15);
  EXPECT_EQ(node_name, "TestNode");

  // Verify port count
  EXPECT_EQ(data[15], 0);  // No ports
}

TEST(DefinitionBinaryBuilderTest, SerializePrimitiveType)
{
  auto data = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Float);

  ASSERT_EQ(data.size(), 2);
  EXPECT_EQ(data[0], static_cast<uint8_t>(TypeKind::Primitive));
  EXPECT_EQ(data[1], static_cast<uint8_t>(PrimitiveTypeId::Float));
}

TEST(DefinitionBinaryBuilderTest, SerializePrimitiveTypeAllTypes)
{
  // Test all primitive types
  auto bool_type = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Bool);
  EXPECT_EQ(bool_type[1], 0x00);

  auto int8_type = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Int8);
  EXPECT_EQ(int8_type[1], 0x01);

  auto uint8_type = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::UInt8);
  EXPECT_EQ(uint8_type[1], 0x02);

  auto float_type = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Float);
  EXPECT_EQ(float_type[1], 0x09);

  auto double_type = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Double);
  EXPECT_EQ(double_type[1], 0x0A);
}

TEST(DefinitionBinaryBuilderTest, SerializeStructType)
{
  std::vector<std::tuple<std::string, PrimitiveTypeId, uint16_t>> fields = {
    {"x", PrimitiveTypeId::Float, 0},     // scalar float
    {"y", PrimitiveTypeId::Float, 0},     // scalar float
    {"data", PrimitiveTypeId::Int32, 10}  // int32[10]
  };

  auto data = DefinitionBinaryBuilder::serializeStructType("Point2D", fields);

  // Type kind (struct)
  EXPECT_EQ(data[0], static_cast<uint8_t>(TypeKind::Struct));

  // Struct name length
  EXPECT_EQ(data[1], 7);  // "Point2D" is 7 characters

  // Struct name
  std::string struct_name(data.begin() + 2, data.begin() + 9);
  EXPECT_EQ(struct_name, "Point2D");

  // Field count
  EXPECT_EQ(data[9], 3);

  // First field "x"
  size_t offset = 10;
  EXPECT_EQ(data[offset], 1);  // field name length
  EXPECT_EQ(data[offset + 1], 'x');
  EXPECT_EQ(data[offset + 2], static_cast<uint8_t>(PrimitiveTypeId::Float));
  EXPECT_EQ(data[offset + 3], 0);  // array size = 0 (scalar)
  EXPECT_EQ(data[offset + 4], 0);

  // Second field "y"
  offset += 5;
  EXPECT_EQ(data[offset], 1);  // field name length
  EXPECT_EQ(data[offset + 1], 'y');
  EXPECT_EQ(data[offset + 2], static_cast<uint8_t>(PrimitiveTypeId::Float));

  // Third field "data" (array)
  offset += 5;
  EXPECT_EQ(data[offset], 4);  // field name length
  std::string field_name(data.begin() + offset + 1, data.begin() + offset + 5);
  EXPECT_EQ(field_name, "data");
  EXPECT_EQ(data[offset + 5], static_cast<uint8_t>(PrimitiveTypeId::Int32));
  EXPECT_EQ(data[offset + 6], 10);  // array size = 10 (little-endian)
  EXPECT_EQ(data[offset + 7], 0);
}

TEST(DefinitionBinaryBuilderTest, AddPortWithPrimitiveType)
{
  DefinitionBinaryBuilder builder;
  builder.begin("SensorNode");

  auto type_data = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Float);
  builder.addPort("temperature", PortDirection::Pub, type_data);

  auto data = builder.build();

  // Should have header + 1 port
  EXPECT_GT(data.size(), 16);  // More than just header

  // Verify port count
  size_t port_count_offset =
    7 + 10;  // after magic(4) + version(2) + name_len(1) + "SensorNode"(10)
  EXPECT_EQ(data[port_count_offset], 1);  // 1 port
}

TEST(DefinitionBinaryBuilderTest, AddPortWithMetadata)
{
  DefinitionBinaryBuilder builder;
  builder.begin("MotorNode");

  auto type_data = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Float);
  std::vector<std::pair<std::string, std::string>> metadata = {
    {"ros_map", "linear.x"}, {"unit", "m/s"}};

  builder.addPort("velocity", PortDirection::Sub, type_data, metadata);

  auto data = builder.build();

  // Verify the data is non-empty and contains metadata
  EXPECT_GT(data.size(), 20);  // Should have substantial data
}

TEST(DefinitionBinaryBuilderTest, AddMultiplePorts)
{
  DefinitionBinaryBuilder builder;
  builder.begin("ControllerNode");

  auto float_type = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Float);
  auto int_type = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Int32);

  builder.addPort("input", PortDirection::Sub, float_type);
  builder.addPort("output", PortDirection::Pub, float_type);
  builder.addPort("status", PortDirection::Pub, int_type);

  auto data = builder.build();

  // Verify port count
  size_t port_count_offset = 7 + 14;      // after header + "ControllerNode"(14)
  EXPECT_EQ(data[port_count_offset], 3);  // 3 ports
}

// ============================================================================
// DefinitionBinaryReader Tests
// ============================================================================

TEST(DefinitionBinaryReaderTest, ParseValidHeader)
{
  DefinitionBinaryBuilder builder;
  builder.begin("TestReader");
  auto data = builder.build();

  DefinitionBinaryReader reader(data);

  std::string node_name;
  uint8_t port_count;

  ASSERT_TRUE(reader.parseHeader(node_name, port_count));
  EXPECT_EQ(node_name, "TestReader");
  EXPECT_EQ(port_count, 0);
}

TEST(DefinitionBinaryReaderTest, ParseHeaderWithPorts)
{
  DefinitionBinaryBuilder builder;
  builder.begin("NodeWithPorts");

  auto type_data = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Float);
  builder.addPort("port1", PortDirection::Pub, type_data);
  builder.addPort("port2", PortDirection::Sub, type_data);

  auto data = builder.build();

  DefinitionBinaryReader reader(data);

  std::string node_name;
  uint8_t port_count;

  ASSERT_TRUE(reader.parseHeader(node_name, port_count));
  EXPECT_EQ(node_name, "NodeWithPorts");
  EXPECT_EQ(port_count, 2);
}

TEST(DefinitionBinaryReaderTest, RejectInvalidMagicNumber)
{
  std::vector<uint8_t> invalid_data = {0x00, 0x00, 0x00, 0x00,  // Wrong magic
                                       0x00, 0x02,              // Version
                                       0x04,                    // Name length
                                       'T',  'e',  's',  't',   // Name
                                       0x00};                   // Port count

  DefinitionBinaryReader reader(invalid_data);

  std::string node_name;
  uint8_t port_count;

  EXPECT_FALSE(reader.parseHeader(node_name, port_count));
}

TEST(DefinitionBinaryReaderTest, RejectInvalidVersion)
{
  std::vector<uint8_t> invalid_data = {0x4C, 0x52, 0x42, 0x46,  // Correct magic "FBRL"
                                       0x00, 0x01,              // Wrong version (0x0100)
                                       0x04,                    // Name length
                                       'T',  'e',  's',  't',   // Name
                                       0x00};                   // Port count

  DefinitionBinaryReader reader(invalid_data);

  std::string node_name;
  uint8_t port_count;

  EXPECT_FALSE(reader.parseHeader(node_name, port_count));
}

TEST(DefinitionBinaryReaderTest, RejectTooShortData)
{
  std::vector<uint8_t> short_data = {0x4C, 0x52, 0x42};  // Only 3 bytes

  DefinitionBinaryReader reader(short_data);

  std::string node_name;
  uint8_t port_count;

  EXPECT_FALSE(reader.parseHeader(node_name, port_count));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(DefinitionBinaryIntegrationTest, RoundTripSimple)
{
  // Build a definition
  DefinitionBinaryBuilder builder;
  builder.begin("IntegrationNode");

  auto float_type = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Float);
  builder.addPort("sensor_data", PortDirection::Pub, float_type);

  auto data = builder.build();

  // Read it back
  DefinitionBinaryReader reader(data);

  std::string node_name;
  uint8_t port_count;

  ASSERT_TRUE(reader.parseHeader(node_name, port_count));
  EXPECT_EQ(node_name, "IntegrationNode");
  EXPECT_EQ(port_count, 1);
}

TEST(DefinitionBinaryIntegrationTest, RoundTripComplex)
{
  // Build a complex definition with struct type
  DefinitionBinaryBuilder builder;
  builder.begin("ComplexNode");

  // Create a struct type
  std::vector<std::tuple<std::string, PrimitiveTypeId, uint16_t>> fields = {
    {"linear_x", PrimitiveTypeId::Float, 0},
    {"linear_y", PrimitiveTypeId::Float, 0},
    {"angular_z", PrimitiveTypeId::Float, 0}};

  auto twist_type = DefinitionBinaryBuilder::serializeStructType("Twist2D", fields);

  std::vector<std::pair<std::string, std::string>> metadata = {
    {"ros_type", "geometry_msgs/msg/Twist"}, {"ros_map", "linear.x"}};

  builder.addPort("cmd_vel", PortDirection::Sub, twist_type, metadata);

  auto float_type = DefinitionBinaryBuilder::serializePrimitiveType(PrimitiveTypeId::Float);
  builder.addPort("battery", PortDirection::Pub, float_type);

  auto data = builder.build();

  // Read it back
  DefinitionBinaryReader reader(data);

  std::string node_name;
  uint8_t port_count;

  ASSERT_TRUE(reader.parseHeader(node_name, port_count));
  EXPECT_EQ(node_name, "ComplexNode");
  EXPECT_EQ(port_count, 2);
}

// ============================================================================
// Constants Tests
// ============================================================================

TEST(DefinitionBinaryConstantsTest, MagicNumberValue)
{
  // Verify magic number is "FBRL"
  EXPECT_EQ(DEFINITION_MAGIC_NUMBER, 0x4642524C);

  // Verify byte order (little-endian)
  uint8_t bytes[4];
  std::memcpy(bytes, &DEFINITION_MAGIC_NUMBER, 4);
  EXPECT_EQ(bytes[0], 0x4C);  // 'L'
  EXPECT_EQ(bytes[1], 0x52);  // 'R'
  EXPECT_EQ(bytes[2], 0x42);  // 'B'
  EXPECT_EQ(bytes[3], 0x46);  // 'F'
}

TEST(DefinitionBinaryConstantsTest, VersionValue)
{
  // Verify version is 0x0200 (v2.0)
  EXPECT_EQ(DEFINITION_VERSION, 0x0200);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

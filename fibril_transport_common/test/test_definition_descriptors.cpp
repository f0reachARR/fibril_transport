#include <gtest/gtest.h>

#include "fibril_transport_common/attribute_system.hpp"
#include "fibril_transport_common/definition_descriptors.hpp"

using namespace fibril_transport_common;

// ========================================
// Port Helper Function Tests
// ========================================

TEST(DefinitionDescriptorsTest, SubPortHelpers)
{
  SubPort port("cmd_vel", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  port.attributes.add(std::make_unique<RosAttribute>("/cmd_vel"));

  Port port_variant = std::move(port);

  EXPECT_EQ(getPortName(port_variant), "cmd_vel");
  EXPECT_EQ(getPortDirection(port_variant), PortDirection::Sub);
  EXPECT_EQ(getPortAttributes(port_variant).size(), 1);
}

TEST(DefinitionDescriptorsTest, PubPortHelpers)
{
  PubPort port("status", TypeDescriptor::makePrimitive(PrimitiveType::Bool));

  Port port_variant = std::move(port);

  EXPECT_EQ(getPortName(port_variant), "status");
  EXPECT_EQ(getPortDirection(port_variant), PortDirection::Pub);
}

TEST(DefinitionDescriptorsTest, ServicePortHelpers)
{
  ServicePort port(
    "trigger", TypeDescriptor::makePrimitive(PrimitiveType::Bool),
    TypeDescriptor::makePrimitive(PrimitiveType::UInt8));

  Port port_variant = std::move(port);

  EXPECT_EQ(getPortName(port_variant), "trigger");
  EXPECT_EQ(getPortDirection(port_variant), PortDirection::Service);
}

// ========================================
// NodeDescriptor Tests
// ========================================

TEST(DefinitionDescriptorsTest, NodeDescriptorBasic)
{
  NodeDescriptor node;
  node.name = "TestNode";

  EXPECT_EQ(node.name, "TestNode");
  EXPECT_TRUE(node.ports.empty());
  EXPECT_TRUE(node.structs.empty());
}

TEST(DefinitionDescriptorsTest, NodeDescriptorWithPorts)
{
  NodeDescriptor node;
  node.name = "MotorController";

  // Add SubPort
  SubPort sub("cmd_velocity", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  sub.attributes.add(std::make_unique<UnitAttribute>("m/s"));
  node.ports.push_back(std::move(sub));

  // Add PubPort
  PubPort pub("current_velocity", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  node.ports.push_back(std::move(pub));

  EXPECT_EQ(node.ports.size(), 2);
  EXPECT_EQ(getPortDirection(node.ports[0]), PortDirection::Sub);
  EXPECT_EQ(getPortDirection(node.ports[1]), PortDirection::Pub);
}

TEST(DefinitionDescriptorsTest, NodeDescriptorFindPort)
{
  NodeDescriptor node;
  node.name = "TestNode";

  SubPort port("test_port", TypeDescriptor::makePrimitive(PrimitiveType::Int32));
  node.ports.push_back(std::move(port));

  auto * found = node.findPort("test_port");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(getPortName(*found), "test_port");

  auto * not_found = node.findPort("does_not_exist");
  EXPECT_EQ(not_found, nullptr);
}

TEST(DefinitionDescriptorsTest, NodeDescriptorFindStruct)
{
  NodeDescriptor node;
  node.name = "TestNode";

  StructDescriptor struct_desc("Twist2D");

  FieldDescriptor field("v", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  struct_desc.fields.push_back(std::move(field));

  node.structs.push_back(std::move(struct_desc));

  auto * found = node.findStruct("Twist2D");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->name, "Twist2D");
  EXPECT_EQ(found->fields.size(), 1);

  auto * not_found = node.findStruct("NonExistent");
  EXPECT_EQ(not_found, nullptr);
}

// ========================================
// Binary Serialization Tests
// ========================================

TEST(DefinitionDescriptorsTest, SimpleNodeSerialization)
{
  // Register attributes for deserialization
  registerBuiltinAttributes();

  NodeDescriptor node;
  node.name = "SimpleNode";

  SubPort port("input", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  node.ports.push_back(std::move(port));

  // Serialize
  auto binary = node.serializeToBinary();

  // Check magic number
  EXPECT_EQ(binary[0], 0x4C);
  EXPECT_EQ(binary[1], 0x52);
  EXPECT_EQ(binary[2], 0x42);
  EXPECT_EQ(binary[3], 0x46);  // "FBRL"

  // Check version
  EXPECT_EQ(binary[4], 0x00);
  EXPECT_EQ(binary[5], 0x02);  // 0x0200

  // Deserialize
  auto deserialized = NodeDescriptor::deserializeFromBinary(binary);

  EXPECT_EQ(deserialized.name, "SimpleNode");
  EXPECT_EQ(deserialized.ports.size(), 1);
  EXPECT_EQ(getPortName(deserialized.ports[0]), "input");
  EXPECT_EQ(getPortDirection(deserialized.ports[0]), PortDirection::Sub);
}

TEST(DefinitionDescriptorsTest, StructTypeSerialization)
{
  registerBuiltinAttributes();

  NodeDescriptor node;
  node.name = "RobotNode";

  // Create struct
  StructDescriptor twist("Twist2D");

  FieldDescriptor v_field("v", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  twist.fields.push_back(std::move(v_field));

  FieldDescriptor w_field("w", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  twist.fields.push_back(std::move(w_field));

  node.structs.push_back(std::move(twist));

  // Add port using the struct
  SubPort port("target_vel", TypeDescriptor::makeStruct("Twist2D"));
  port.attributes.add(std::make_unique<RosTypeAttribute>("geometry_msgs/msg/Twist"));
  node.ports.push_back(std::move(port));

  // Round-trip
  auto binary = node.serializeToBinary();
  auto deserialized = NodeDescriptor::deserializeFromBinary(binary);

  EXPECT_EQ(deserialized.name, "RobotNode");
  EXPECT_EQ(deserialized.structs.size(), 1);
  EXPECT_EQ(deserialized.structs[0].name, "Twist2D");
  EXPECT_EQ(deserialized.structs[0].fields.size(), 2);
  EXPECT_EQ(deserialized.ports.size(), 1);
}

TEST(DefinitionDescriptorsTest, ArrayTypeSerialization)
{
  registerBuiltinAttributes();

  NodeDescriptor node;
  node.name = "ArrayNode";

  // Create struct for struct array
  StructDescriptor struct_data("Data");

  FieldDescriptor struct_field("data", TypeDescriptor::makePrimitive(PrimitiveType::Int32));
  struct_data.fields.push_back(std::move(struct_field));

  node.structs.push_back(std::move(struct_data));

  // Create struct with array field
  StructDescriptor data_struct("DataArray");

  FieldDescriptor array_primitive_field(
    "values", TypeDescriptor::makeArray(TypeDescriptor::makePrimitive(PrimitiveType::Int32), 10));
  data_struct.fields.push_back(std::move(array_primitive_field));

  FieldDescriptor array_struct_field(
    "data", TypeDescriptor::makeArray(TypeDescriptor::makeStruct("Data"), 10));
  data_struct.fields.push_back(std::move(array_struct_field));

  node.structs.push_back(std::move(data_struct));

  SubPort port("data_input", TypeDescriptor::makeStruct("DataArray"));
  node.ports.push_back(std::move(port));

  // Round-trip
  auto binary = node.serializeToBinary();
  auto deserialized = NodeDescriptor::deserializeFromBinary(binary);

  EXPECT_EQ(deserialized.structs.size(), 2);
  EXPECT_EQ(deserialized.structs[0].fields.size(), 1);
  EXPECT_TRUE(deserialized.structs[0].fields[0].type.isPrimitive());

  EXPECT_EQ(deserialized.structs[1].fields.size(), 2);
  EXPECT_TRUE(deserialized.structs[1].fields[0].type.isArray());
  EXPECT_EQ(deserialized.structs[1].fields[0].type.arraySize(), 10);
  EXPECT_TRUE(deserialized.structs[1].fields[0].type.arrayElement().isPrimitive());

  EXPECT_TRUE(deserialized.structs[1].fields[1].type.isArray());
  EXPECT_EQ(deserialized.structs[1].fields[1].type.arraySize(), 10);
  EXPECT_TRUE(deserialized.structs[1].fields[1].type.arrayElement().isStruct());
}

TEST(DefinitionDescriptorsTest, ServicePortSerialization)
{
  registerBuiltinAttributes();

  NodeDescriptor node;
  node.name = "ServiceNode";

  // Create request struct
  StructDescriptor request_struct("TriggerRequest");

  FieldDescriptor req_field("enable", TypeDescriptor::makePrimitive(PrimitiveType::Bool));
  request_struct.fields.push_back(std::move(req_field));

  node.structs.push_back(std::move(request_struct));

  // Create response struct
  StructDescriptor response_struct("TriggerResponse");

  FieldDescriptor res_field("success", TypeDescriptor::makePrimitive(PrimitiveType::Bool));
  response_struct.fields.push_back(std::move(res_field));

  node.structs.push_back(std::move(response_struct));

  // Add service port
  ServicePort service(
    "trigger_service", TypeDescriptor::makeStruct("TriggerRequest"),
    TypeDescriptor::makeStruct("TriggerResponse"));
  service.attributes.add(std::make_unique<RosAttribute>("~/trigger"));
  node.ports.push_back(std::move(service));

  // Round-trip
  auto binary = node.serializeToBinary();
  auto deserialized = NodeDescriptor::deserializeFromBinary(binary);

  EXPECT_EQ(deserialized.name, "ServiceNode");
  EXPECT_EQ(deserialized.ports.size(), 1);
  EXPECT_EQ(getPortDirection(deserialized.ports[0]), PortDirection::Service);

  // Verify service port content
  auto & service_port = std::get<ServicePort>(deserialized.ports[0]);
  EXPECT_EQ(service_port.name, "trigger_service");
  EXPECT_TRUE(service_port.request_type.isStruct());
  EXPECT_EQ(service_port.request_type.asStructName(), "TriggerRequest");
  EXPECT_TRUE(service_port.response_type.isStruct());
  EXPECT_EQ(service_port.response_type.asStructName(), "TriggerResponse");
}

TEST(DefinitionDescriptorsTest, PortAttributesSerialization)
{
  registerBuiltinAttributes();

  NodeDescriptor node;
  node.name = "AttributeTestNode";

  SubPort port("velocity", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  port.attributes.add(std::make_unique<RosAttribute>("/cmd_vel"));
  port.attributes.add(std::make_unique<UnitAttribute>("m/s"));
  port.attributes.add(std::make_unique<RosMapAttribute>("linear.x"));
  node.ports.push_back(std::move(port));

  // Round-trip
  auto binary = node.serializeToBinary();
  auto deserialized = NodeDescriptor::deserializeFromBinary(binary);

  EXPECT_EQ(deserialized.ports.size(), 1);

  auto & attrs = getPortAttributes(deserialized.ports[0]);
  EXPECT_EQ(attrs.size(), 3);

  auto * ros_attr = attrs.find<RosAttribute>();
  ASSERT_NE(ros_attr, nullptr);
  EXPECT_EQ(ros_attr->topic_name, "/cmd_vel");

  auto * unit_attr = attrs.find<UnitAttribute>();
  ASSERT_NE(unit_attr, nullptr);
  EXPECT_EQ(unit_attr->unit, "m/s");
}

TEST(DefinitionDescriptorsTest, ComplexNodeSerialization)
{
  registerBuiltinAttributes();

  NodeDescriptor node;
  node.name = "ComplexNode";

  // Add multiple structs
  StructDescriptor struct1("Pose");
  FieldDescriptor x("x", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  x.attributes.add(std::make_unique<UnitAttribute>("m"));
  x.attributes.add(std::make_unique<RosMapAttribute>("pose.x"));
  struct1.fields.push_back(std::move(x));

  struct1.attributes.add(std::make_unique<RosTypeAttribute>("geometry_msgs/msg/Pose"));
  node.structs.push_back(std::move(struct1));

  // Add mixed port types
  SubPort sub("cmd", TypeDescriptor::makeStruct("Pose"));
  sub.attributes.add(std::make_unique<RosAttribute>("/cmd_pose"));
  node.ports.push_back(std::move(sub));

  PubPort pub("status", TypeDescriptor::makePrimitive(PrimitiveType::Bool));
  pub.attributes.add(std::make_unique<RosAttribute>("/status"));
  node.ports.push_back(std::move(pub));

  ServicePort svc(
    "reset", TypeDescriptor::makePrimitive(PrimitiveType::Bool),
    TypeDescriptor::makePrimitive(PrimitiveType::UInt8));
  node.ports.push_back(std::move(svc));

  // Round-trip
  auto binary = node.serializeToBinary();
  auto deserialized = NodeDescriptor::deserializeFromBinary(binary);

  EXPECT_EQ(deserialized.name, "ComplexNode");
  EXPECT_EQ(deserialized.structs.size(), 1);
  EXPECT_EQ(deserialized.ports.size(), 3);
  EXPECT_EQ(getPortDirection(deserialized.ports[0]), PortDirection::Sub);
  EXPECT_EQ(getPortDirection(deserialized.ports[1]), PortDirection::Pub);
  EXPECT_EQ(getPortDirection(deserialized.ports[2]), PortDirection::Service);

  // Inner struct attributes
  EXPECT_EQ(deserialized.structs[0].attributes.size(), 1);
  ASSERT_NE(deserialized.structs[0].attributes.find<RosTypeAttribute>(), nullptr);
  EXPECT_EQ(
    deserialized.structs[0].attributes.find<RosTypeAttribute>()->ros_message_type,
    "geometry_msgs/msg/Pose");
  EXPECT_EQ(deserialized.structs[0].fields.size(), 1);
  EXPECT_EQ(deserialized.structs[0].fields[0].name, "x");
  ASSERT_NE(deserialized.structs[0].fields[0].attributes.find<RosMapAttribute>(), nullptr);
  ASSERT_NE(deserialized.structs[0].fields[0].attributes.find<UnitAttribute>(), nullptr);
  EXPECT_EQ(
    deserialized.structs[0].fields[0].attributes.find<RosMapAttribute>()->field_path, "pose.x");
  EXPECT_EQ(deserialized.structs[0].fields[0].attributes.find<UnitAttribute>()->unit, "m");

  // Inner struct
  EXPECT_EQ(deserialized.structs[0].name, "Pose");
  EXPECT_EQ(deserialized.structs[0].fields[0].name, "x");

  // Port attributes
  const auto & sub_port = std::get<SubPort>(deserialized.ports[0]);
  EXPECT_EQ(sub_port.attributes.size(), 1);
  ASSERT_NE(sub_port.attributes.find<RosAttribute>(), nullptr);
  EXPECT_EQ(sub_port.attributes.find<RosAttribute>()->topic_name, "/cmd_pose");

  // Port name / type
  EXPECT_EQ(sub_port.name, "cmd");
  EXPECT_EQ(sub_port.data_type.isStruct(), true);
  EXPECT_EQ(sub_port.data_type.asStructName(), "Pose");
}

// ========================================
// Checksum Tests
// ========================================

TEST(DefinitionDescriptorsTest, ChecksumCalculation)
{
  NodeDescriptor node;
  node.name = "ChecksumNode";

  SubPort port("test", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  node.ports.push_back(std::move(port));

  uint32_t checksum1 = node.calculateChecksum();
  uint32_t checksum2 = node.calculateChecksum();

  // Same node should have same checksum
  EXPECT_EQ(checksum1, checksum2);
  EXPECT_NE(checksum1, 0);
}

TEST(DefinitionDescriptorsTest, ChecksumDifferent)
{
  NodeDescriptor node1;
  node1.name = "Node1";
  SubPort port1("port1", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  node1.ports.push_back(std::move(port1));

  NodeDescriptor node2;
  node2.name = "Node2";  // Different name
  SubPort port2("port1", TypeDescriptor::makePrimitive(PrimitiveType::Float));
  node2.ports.push_back(std::move(port2));

  // Different nodes should have different checksums
  EXPECT_NE(node1.calculateChecksum(), node2.calculateChecksum());
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

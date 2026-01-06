#include <gtest/gtest.h>

#include "fibril_transport_common/attribute_system.hpp"
#include "fibril_transport_common/ros_attribute.hpp"

using namespace fibril_transport_common;

// ========================================
// RosTypeAttribute Tests
// ========================================

TEST(AttributeSystemTest, RosTypeAttribute)
{
  RosTypeAttribute attr("geometry_msgs/msg/Twist");

  EXPECT_EQ(attr.name(), "ros_type");
  EXPECT_EQ(attr.ros_message_type, "geometry_msgs/msg/Twist");

  // Test serialization
  auto binary = attr.serializeToBinary();
  EXPECT_FALSE(binary.empty());

  // Test deserialization
  auto deserialized = RosTypeAttribute::fromBinary(binary);
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->ros_message_type, "geometry_msgs/msg/Twist");
}

// ========================================
// RosMapAttribute Tests
// ========================================

TEST(AttributeSystemTest, RosMapAttribute)
{
  RosMapAttribute attr("linear.x");

  EXPECT_EQ(attr.name(), "ros_map");
  EXPECT_EQ(attr.field_path, "linear.x");

  // Test round-trip
  auto binary = attr.serializeToBinary();
  auto deserialized = RosMapAttribute::fromBinary(binary);
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->field_path, "linear.x");
}

// ========================================
// RosAttribute Tests
// ========================================

TEST(AttributeSystemTest, RosAttribute)
{
  RosAttribute attr("/cmd_vel");

  EXPECT_EQ(attr.name(), "ros");
  EXPECT_EQ(attr.topic_name, "/cmd_vel");

  // Test round-trip
  auto binary = attr.serializeToBinary();
  auto deserialized = RosAttribute::fromBinary(binary);
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->topic_name, "/cmd_vel");
}

// ========================================
// UnitAttribute Tests
// ========================================

TEST(AttributeSystemTest, UnitAttribute)
{
  UnitAttribute attr("m/s");

  EXPECT_EQ(attr.name(), "unit");
  EXPECT_EQ(attr.unit, "m/s");

  // Test round-trip
  auto binary = attr.serializeToBinary();
  auto deserialized = UnitAttribute::fromBinary(binary);
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->unit, "m/s");
}

// ========================================
// RosFrameIdAttribute Tests
// ========================================

TEST(AttributeSystemTest, RosFrameIdAttribute)
{
  RosFrameIdAttribute attr("base_link");

  EXPECT_EQ(attr.name(), "ros_frame_id");
  EXPECT_EQ(attr.frame_id, "base_link");

  // Test round-trip
  auto binary = attr.serializeToBinary();
  auto deserialized = RosFrameIdAttribute::fromBinary(binary);
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->frame_id, "base_link");
}

// ========================================
// AttributeList Tests
// ========================================

TEST(AttributeSystemTest, AttributeListAdd)
{
  AttributeList list;

  list.add(std::make_unique<RosTypeAttribute>("std_msgs/msg/Float32"));
  list.add(std::make_unique<UnitAttribute>("m/s"));

  EXPECT_EQ(list.size(), 2);
  EXPECT_FALSE(list.empty());
}

TEST(AttributeSystemTest, AttributeListFind)
{
  AttributeList list;

  list.add(std::make_unique<RosTypeAttribute>("std_msgs/msg/Float32"));
  list.add(std::make_unique<UnitAttribute>("m/s"));
  list.add(std::make_unique<RosMapAttribute>("data"));

  // Find specific type
  auto * ros_type = list.find<RosTypeAttribute>();
  ASSERT_NE(ros_type, nullptr);
  EXPECT_EQ(ros_type->ros_message_type, "std_msgs/msg/Float32");

  auto * unit = list.find<UnitAttribute>();
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(unit->unit, "m/s");

  // Find non-existent type
  auto * frame_id = list.find<RosFrameIdAttribute>();
  EXPECT_EQ(frame_id, nullptr);
}

TEST(AttributeSystemTest, AttributeListFindAll)
{
  AttributeList list;

  list.add(std::make_unique<RosMapAttribute>("linear.x"));
  list.add(std::make_unique<RosMapAttribute>("angular.z"));
  list.add(std::make_unique<UnitAttribute>("m/s"));

  auto ros_maps = list.findAll<RosMapAttribute>();
  EXPECT_EQ(ros_maps.size(), 2);
  EXPECT_EQ(ros_maps[0]->field_path, "linear.x");
  EXPECT_EQ(ros_maps[1]->field_path, "angular.z");
}

TEST(AttributeSystemTest, AttributeListSerialization)
{
  AttributeList list;

  list.add(std::make_unique<RosTypeAttribute>("geometry_msgs/msg/Twist"));
  list.add(std::make_unique<UnitAttribute>("m/s"));
  list.add(std::make_unique<RosMapAttribute>("linear.x"));

  // Serialize
  auto binary = list.serializeToBinary();
  EXPECT_FALSE(binary.empty());

  // Deserialize
  registerBuiltinAttributes();  // Register deserializers
  size_t offset = 0;
  auto deserialized = AttributeList::deserializeFromBinary(binary, offset);

  EXPECT_EQ(deserialized.size(), 3);

  // Verify contents
  auto * ros_type = deserialized.find<RosTypeAttribute>();
  ASSERT_NE(ros_type, nullptr);
  EXPECT_EQ(ros_type->ros_message_type, "geometry_msgs/msg/Twist");

  auto * unit = deserialized.find<UnitAttribute>();
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(unit->unit, "m/s");
}

TEST(AttributeSystemTest, AttributeListMoveOnly)
{
  AttributeList list1;
  list1.add(std::make_unique<UnitAttribute>("m/s"));

  // Move construction
  AttributeList list2 = std::move(list1);
  EXPECT_EQ(list2.size(), 1);

  // Move assignment
  AttributeList list3;
  list3 = std::move(list2);
  EXPECT_EQ(list3.size(), 1);
}

// ========================================
// AttributeFactory Tests
// ========================================

TEST(AttributeSystemTest, AttributeFactory)
{
  registerBuiltinAttributes();

  // Test RosType
  std::vector<uint8_t> ros_type_data = {'T', 'w', 'i', 's', 't'};
  auto attr = AttributeFactory::fromBinary("ros_type", ros_type_data);
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->name(), "ros_type");

  // Test unknown attribute
  auto unknown = AttributeFactory::fromBinary("unknown_attr", ros_type_data);
  EXPECT_EQ(unknown, nullptr);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
